// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>
#include <string>
#include <vector>

#include <wrl/client.h>
#include <wrl/implements.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/win/scoped_hstring.h"
#include "base/win/windows_version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_platform_bridge_win.h"
#include "chrome/browser/notifications/win/fake_itoastnotification.h"
#include "chrome/browser/notifications/win/fake_itoastnotifier.h"
#include "chrome/browser/notifications/win/notification_launch_id.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"

namespace mswr = Microsoft::WRL;
namespace winui = ABI::Windows::UI;

namespace {

const char kLaunchId[] = "0|0|Default|0|https://example.com/|notification_id";
const char kLaunchIdButtonClick[] =
    "1|0|0|Default|0|https://example.com/|notification_id";
const char kLaunchIdSettings[] =
    "2|0|Default|0|https://example.com/|notification_id";

// Windows native notification have a dependency on WinRT (Win 8+) and the
// built in Notification Center. Although native notifications in Chrome are
// only available in Win 10+ we keep the minimum test coverage at Win 8 in case
// we decide to backport.
constexpr base::win::Version kMinimumWindowsVersion = base::win::Version::WIN8;

Profile* CreateTestingProfile(const base::FilePath& path) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  size_t starting_number_of_profiles = profile_manager->GetNumberOfProfiles();

  if (!base::PathExists(path) && !base::CreateDirectory(path))
    NOTREACHED() << "Could not create directory at " << path.MaybeAsASCII();

  std::unique_ptr<Profile> profile =
      Profile::CreateProfile(path, nullptr, Profile::CREATE_MODE_SYNCHRONOUS);
  Profile* profile_ptr = profile.get();
  profile_manager->RegisterTestingProfile(std::move(profile), true, false);
  EXPECT_EQ(starting_number_of_profiles + 1,
            profile_manager->GetNumberOfProfiles());
  return profile_ptr;
}

Profile* CreateTestingProfile(const std::string& profile_name) {
  base::FilePath path;
  base::PathService::Get(chrome::DIR_USER_DATA, &path);
  path = path.AppendASCII(profile_name);
  return CreateTestingProfile(path);
}

base::string16 GetToastString(const base::string16& notification_id,
                              const base::string16& profile_id,
                              bool incognito) {
  return base::StringPrintf(
      LR"(<toast launch="0|0|%ls|%d|https://foo.com/|%ls"></toast>)",
      profile_id.c_str(), incognito, notification_id.c_str());
}

}  // namespace

class NotificationPlatformBridgeWinUITest : public InProcessBrowserTest {
 public:
  NotificationPlatformBridgeWinUITest() = default;
  ~NotificationPlatformBridgeWinUITest() override = default;

  void SetUpOnMainThread() override {
    display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(
            browser()->profile());
  }

  NotificationPlatformBridgeWin* GetBridge() {
    return static_cast<NotificationPlatformBridgeWin*>(
        g_browser_process->notification_platform_bridge());
  }

  void TearDownOnMainThread() override { display_service_tester_.reset(); }

  void HandleOperation(const base::RepeatingClosure& quit_task,
                       NotificationCommon::Operation operation,
                       NotificationHandler::Type notification_type,
                       const GURL& origin,
                       const std::string& notification_id,
                       const base::Optional<int>& action_index,
                       const base::Optional<base::string16>& reply,
                       const base::Optional<bool>& by_user) {
    last_operation_ = operation;
    last_notification_type_ = notification_type;
    last_origin_ = origin;
    last_notification_id_ = notification_id;
    last_action_index_ = action_index;
    last_reply_ = reply;
    last_by_user_ = by_user;
    quit_task.Run();
  }

  void DisplayedNotifications(const base::RepeatingClosure& quit_task,
                              std::set<std::string> displayed_notifications,
                              bool supports_synchronization) {
    displayed_notifications_ = std::move(displayed_notifications);
    quit_task.Run();
  }

  void ValidateLaunchId(const std::string& expected_launch_id,
                        const base::RepeatingClosure& quit_task,
                        const NotificationLaunchId& launch_id) {
    ASSERT_TRUE(launch_id.is_valid());
    ASSERT_STREQ(expected_launch_id.c_str(), launch_id.Serialize().c_str());
    quit_task.Run();
  }

 protected:
  void ProcessLaunchIdViaCmdLine(const std::string& launch_id,
                                 const std::string& inline_reply) {
    base::RunLoop run_loop;
    display_service_tester_->SetProcessNotificationOperationDelegate(
        base::BindRepeating(
            &NotificationPlatformBridgeWinUITest::HandleOperation,
            base::Unretained(this), run_loop.QuitClosure()));

    // Simulate clicks on the toast.
    base::CommandLine command_line = base::CommandLine::FromString(L"");
    command_line.AppendSwitchASCII(switches::kNotificationLaunchId, launch_id);
    if (!inline_reply.empty()) {
      command_line.AppendSwitchASCII(switches::kNotificationInlineReply,
                                     inline_reply);
    }
    ASSERT_TRUE(NotificationPlatformBridgeWin::HandleActivation(command_line));

    run_loop.Run();
  }

  bool ValidateNotificationValues(NotificationCommon::Operation operation,
                                  NotificationHandler::Type notification_type,
                                  const GURL& origin,
                                  const std::string& notification_id,
                                  const base::Optional<int>& action_index,
                                  const base::Optional<base::string16>& reply,
                                  const base::Optional<bool>& by_user) {
    return operation == last_operation_ &&
           notification_type == last_notification_type_ &&
           origin == last_origin_ && notification_id == last_notification_id_ &&
           action_index == last_action_index_ && reply == last_reply_ &&
           by_user == last_by_user_;
  }

  std::unique_ptr<NotificationDisplayServiceTester> display_service_tester_;

  NotificationCommon::Operation last_operation_;
  NotificationHandler::Type last_notification_type_;
  GURL last_origin_;
  std::string last_notification_id_;
  base::Optional<int> last_action_index_;
  base::Optional<base::string16> last_reply_;
  base::Optional<bool> last_by_user_;

  std::set<std::string> displayed_notifications_;

  bool delegate_called_ = false;
};

class FakeIToastActivatedEventArgs
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          winui::Notifications::IToastActivatedEventArgs> {
 public:
  explicit FakeIToastActivatedEventArgs(const base::string16& args)
      : arguments_(args) {}
  ~FakeIToastActivatedEventArgs() override = default;

  HRESULT STDMETHODCALLTYPE get_Arguments(HSTRING* value) override {
    base::win::ScopedHString arguments =
        base::win::ScopedHString::Create(arguments_);
    *value = arguments.get();
    return S_OK;
  }

 private:
  base::string16 arguments_;

  DISALLOW_COPY_AND_ASSIGN(FakeIToastActivatedEventArgs);
};

IN_PROC_BROWSER_TEST_F(NotificationPlatformBridgeWinUITest, HandleEvent) {
  if (base::win::GetVersion() < kMinimumWindowsVersion)
    return;

  const wchar_t kXmlDoc[] =
      LR"(<toast launch="0|0|Default|0|https://example.com/|notification_id">
 <visual>
  <binding template="ToastGeneric">
   <text>My Title</text>
   <text placement="attribution">example.com</text>
  </binding>
 </visual>
 <actions>
 <action content="Click" arguments="args" activationType="foreground"/>
 </actions>
</toast>
)";

  FakeIToastNotification toast(kXmlDoc, L"tag");
  FakeIToastActivatedEventArgs args(
      L"1|1|0|Default|0|https://example.com/|notification_id");

  base::RunLoop run_loop;
  display_service_tester_->SetProcessNotificationOperationDelegate(
      base::BindRepeating(&NotificationPlatformBridgeWinUITest::HandleOperation,
                          base::Unretained(this), run_loop.QuitClosure()));

  // Simulate clicks on the toast.
  NotificationPlatformBridgeWin* bridge = GetBridge();
  ASSERT_TRUE(bridge);
  bridge->ForwardHandleEventForTesting(NotificationCommon::OPERATION_CLICK,
                                       &toast, &args, base::nullopt);
  run_loop.Run();

  // Validate the click values.
  EXPECT_EQ(NotificationCommon::OPERATION_CLICK, last_operation_);
  EXPECT_EQ(NotificationHandler::Type::WEB_PERSISTENT, last_notification_type_);
  EXPECT_EQ(GURL("https://example.com/"), last_origin_);
  EXPECT_EQ("notification_id", last_notification_id_);
  EXPECT_EQ(1, last_action_index_);
  EXPECT_EQ(base::nullopt, last_reply_);
  EXPECT_EQ(base::nullopt, last_by_user_);
}

IN_PROC_BROWSER_TEST_F(NotificationPlatformBridgeWinUITest, HandleActivation) {
  if (base::win::GetVersion() < kMinimumWindowsVersion)
    return;

  base::RunLoop run_loop;
  display_service_tester_->SetProcessNotificationOperationDelegate(
      base::BindRepeating(&NotificationPlatformBridgeWinUITest::HandleOperation,
                          base::Unretained(this), run_loop.QuitClosure()));

  // Simulate notification activation.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchNative(
      switches::kNotificationLaunchId,
      L"1|1|0|Default|0|https://example.com/|notification_id");
  NotificationPlatformBridgeWin::HandleActivation(command_line);
  run_loop.Run();

  // Validate the values.
  EXPECT_EQ(NotificationCommon::OPERATION_CLICK, last_operation_);
  EXPECT_EQ(NotificationHandler::Type::WEB_PERSISTENT, last_notification_type_);
  EXPECT_EQ(GURL("https://example.com/"), last_origin_);
  EXPECT_EQ("notification_id", last_notification_id_);
  EXPECT_EQ(1, last_action_index_);
  EXPECT_EQ(base::nullopt, last_reply_);
  EXPECT_EQ(true, last_by_user_);
}

IN_PROC_BROWSER_TEST_F(NotificationPlatformBridgeWinUITest, HandleSettings) {
  if (base::win::GetVersion() < kMinimumWindowsVersion)
    return;

  const wchar_t kXmlDoc[] =
      LR"(<toast launch="0|0|Default|0|https://example.com/|notification_id">
 <visual>
  <binding template="ToastGeneric">
   <text>My Title</text>
   <text placement="attribution">example.com</text>
  </binding>
 </visual>
 <actions>
   <action content="settings" placement="contextMenu" activationType="foreground" arguments="2|0|Default|0|https://example.com/|notification_id"/>
 </actions>
</toast>
)";

  FakeIToastNotification toast(kXmlDoc, L"tag");
  FakeIToastActivatedEventArgs args(
      L"2|0|Default|0|https://example.com/|notification_id");

  base::RunLoop run_loop;
  display_service_tester_->SetProcessNotificationOperationDelegate(
      base::BindRepeating(&NotificationPlatformBridgeWinUITest::HandleOperation,
                          base::Unretained(this), run_loop.QuitClosure()));

  // Simulate clicks on the toast.
  NotificationPlatformBridgeWin* bridge = GetBridge();
  ASSERT_TRUE(bridge);
  bridge->ForwardHandleEventForTesting(NotificationCommon::OPERATION_SETTINGS,
                                       &toast, &args, base::nullopt);
  run_loop.Run();

  // Validate the click values.
  EXPECT_EQ(NotificationCommon::OPERATION_SETTINGS, last_operation_);
  EXPECT_EQ(NotificationHandler::Type::WEB_PERSISTENT, last_notification_type_);
  EXPECT_EQ(GURL("https://example.com/"), last_origin_);
  EXPECT_EQ("notification_id", last_notification_id_);
  EXPECT_EQ(base::nullopt, last_action_index_);
  EXPECT_EQ(base::nullopt, last_reply_);
  EXPECT_EQ(base::nullopt, last_by_user_);
}

IN_PROC_BROWSER_TEST_F(NotificationPlatformBridgeWinUITest, HandleClose) {
  if (base::win::GetVersion() < kMinimumWindowsVersion)
    return;

  base::RunLoop run_loop;
  display_service_tester_->SetProcessNotificationOperationDelegate(
      base::BindRepeating(&NotificationPlatformBridgeWinUITest::HandleOperation,
                          base::Unretained(this), run_loop.QuitClosure()));

  // Simulate notification close.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchNative(
      switches::kNotificationLaunchId,
      L"3|0|Default|0|https://example.com/|notification_id");
  NotificationPlatformBridgeWin::HandleActivation(command_line);
  run_loop.Run();

  // Validate the values.
  EXPECT_EQ(NotificationCommon::OPERATION_CLOSE, last_operation_);
  EXPECT_EQ(NotificationHandler::Type::WEB_PERSISTENT, last_notification_type_);
  EXPECT_EQ(GURL("https://example.com/"), last_origin_);
  EXPECT_EQ("notification_id", last_notification_id_);
  EXPECT_EQ(base::nullopt, last_action_index_);
  EXPECT_EQ(base::nullopt, last_reply_);
  EXPECT_EQ(true, last_by_user_);
}

IN_PROC_BROWSER_TEST_F(NotificationPlatformBridgeWinUITest, GetDisplayed) {
  if (base::win::GetVersion() < kMinimumWindowsVersion)
    return;

  NotificationPlatformBridgeWin* bridge = GetBridge();
  ASSERT_TRUE(bridge);

  std::vector<mswr::ComPtr<winui::Notifications::IToastNotification>>
      notifications;
  bridge->SetDisplayedNotificationsForTesting(&notifications);

  // Validate that empty list of notifications show 0 results.
  {
    base::RunLoop run_loop;
    bridge->GetDisplayed(
        browser()->profile(),
        base::BindRepeating(
            &NotificationPlatformBridgeWinUITest::DisplayedNotifications,
            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_EQ(0U, displayed_notifications_.size());
  }

  // Add four items (two in each profile, one for each being incognito and one
  // for each that is not).
  bool incognito = true;

  Profile* profile1 = CreateTestingProfile("P1");
  notifications.push_back(Microsoft::WRL::Make<FakeIToastNotification>(
      GetToastString(L"P1i", L"P1", incognito), L"tag"));
  notifications.push_back(Microsoft::WRL::Make<FakeIToastNotification>(
      GetToastString(L"P1reg", L"P1", !incognito), L"tag"));

  Profile* profile2 = CreateTestingProfile("P2");
  notifications.push_back(Microsoft::WRL::Make<FakeIToastNotification>(
      GetToastString(L"P2i", L"P2", incognito), L"tag"));
  notifications.push_back(Microsoft::WRL::Make<FakeIToastNotification>(
      GetToastString(L"P2reg", L"P2", !incognito), L"tag"));

  // Query for profile P1 in incognito (should return 1 item).
  {
    base::RunLoop run_loop;
    bridge->GetDisplayed(
        profile1->GetOffTheRecordProfile(),
        base::BindRepeating(
            &NotificationPlatformBridgeWinUITest::DisplayedNotifications,
            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_EQ(1U, displayed_notifications_.size());
    EXPECT_EQ(1U, displayed_notifications_.count("P1i"));
  }

  // Query for profile P1 not in incognito (should return 1 item).
  {
    base::RunLoop run_loop;
    bridge->GetDisplayed(
        profile1,
        base::BindRepeating(
            &NotificationPlatformBridgeWinUITest::DisplayedNotifications,
            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_EQ(1U, displayed_notifications_.size());
    EXPECT_EQ(1U, displayed_notifications_.count("P1reg"));
  }

  // Query for profile P2 in incognito (should return 1 item).
  {
    base::RunLoop run_loop;
    bridge->GetDisplayed(
        profile2->GetOffTheRecordProfile(),
        base::BindRepeating(
            &NotificationPlatformBridgeWinUITest::DisplayedNotifications,
            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_EQ(1U, displayed_notifications_.size());
    EXPECT_EQ(1U, displayed_notifications_.count("P2i"));
  }

  // Query for profile P2 not in incognito (should return 1 item).
  {
    base::RunLoop run_loop;
    bridge->GetDisplayed(
        profile2,
        base::BindRepeating(
            &NotificationPlatformBridgeWinUITest::DisplayedNotifications,
            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_EQ(1U, displayed_notifications_.size());
    EXPECT_EQ(1U, displayed_notifications_.count("P2reg"));
  }

  bridge->SetDisplayedNotificationsForTesting(nullptr);
}

// Test calling Display with a fake implementation of the Action Center
// and validate it gets the values expected.
IN_PROC_BROWSER_TEST_F(NotificationPlatformBridgeWinUITest, DisplayWithFakeAC) {
  if (base::win::GetVersion() < kMinimumWindowsVersion)
    return;

  NotificationPlatformBridgeWin* bridge = GetBridge();
  ASSERT_TRUE(bridge);

  FakeIToastNotifier notifier;
  bridge->SetNotifierForTesting(&notifier);

  std::string launch_id_value = "0|0|P1|0|https://example.com/|notification_id";
  NotificationLaunchId launch_id(launch_id_value);
  ASSERT_TRUE(launch_id.is_valid());

  auto notification = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE, "notification_id", L"Text1",
      L"Text2", gfx::Image(), base::string16(), GURL("https://example.com/"),
      message_center::NotifierId(), message_center::RichNotificationData(),
      nullptr);

  std::unique_ptr<NotificationCommon::Metadata> metadata;
  Profile* profile = CreateTestingProfile("P1");

  {
    base::RunLoop run_loop;
    notifier.SetNotificationShownCallback(base::BindRepeating(
        &NotificationPlatformBridgeWinUITest::ValidateLaunchId,
        base::Unretained(this), launch_id_value, run_loop.QuitClosure()));
    bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile,
                    *notification, std::move(metadata));
    run_loop.Run();
  }

  bridge->SetNotifierForTesting(nullptr);
}

IN_PROC_BROWSER_TEST_F(NotificationPlatformBridgeWinUITest, CmdLineClick) {
  if (base::win::GetVersion() < kMinimumWindowsVersion)
    return;

  ASSERT_NO_FATAL_FAILURE(ProcessLaunchIdViaCmdLine(kLaunchId, /*reply=*/""));

  // Validate the click values.
  EXPECT_EQ(NotificationCommon::OPERATION_CLICK, last_operation_);
  EXPECT_EQ(NotificationHandler::Type::WEB_PERSISTENT, last_notification_type_);
  EXPECT_EQ(GURL("https://example.com/"), last_origin_);
  EXPECT_EQ("notification_id", last_notification_id_);
  EXPECT_EQ(base::nullopt, last_action_index_);
  EXPECT_EQ(base::nullopt, last_reply_);
  EXPECT_TRUE(last_by_user_);
}

IN_PROC_BROWSER_TEST_F(NotificationPlatformBridgeWinUITest,
                       CmdLineInlineReply) {
  if (base::win::GetVersion() < kMinimumWindowsVersion)
    return;

  ASSERT_NO_FATAL_FAILURE(
      ProcessLaunchIdViaCmdLine(kLaunchIdButtonClick, "Inline reply"));

  // Validate the click values.
  EXPECT_EQ(NotificationCommon::OPERATION_CLICK, last_operation_);
  EXPECT_EQ(NotificationHandler::Type::WEB_PERSISTENT, last_notification_type_);
  EXPECT_EQ(GURL("https://example.com/"), last_origin_);
  EXPECT_EQ("notification_id", last_notification_id_);
  EXPECT_EQ(0, last_action_index_);
  EXPECT_EQ(L"Inline reply", last_reply_);
  EXPECT_TRUE(last_by_user_);
}

IN_PROC_BROWSER_TEST_F(NotificationPlatformBridgeWinUITest, CmdLineButton) {
  if (base::win::GetVersion() < kMinimumWindowsVersion)
    return;

  ASSERT_NO_FATAL_FAILURE(
      ProcessLaunchIdViaCmdLine(kLaunchIdButtonClick, /*reply=*/""));

  // Validate the click values.
  EXPECT_EQ(NotificationCommon::OPERATION_CLICK, last_operation_);
  EXPECT_EQ(NotificationHandler::Type::WEB_PERSISTENT, last_notification_type_);
  EXPECT_EQ(GURL("https://example.com/"), last_origin_);
  EXPECT_EQ("notification_id", last_notification_id_);
  EXPECT_EQ(0, last_action_index_);
  EXPECT_EQ(base::nullopt, last_reply_);
  EXPECT_TRUE(last_by_user_);
}

IN_PROC_BROWSER_TEST_F(NotificationPlatformBridgeWinUITest, CmdLineSettings) {
  if (base::win::GetVersion() < kMinimumWindowsVersion)
    return;

  ASSERT_NO_FATAL_FAILURE(
      ProcessLaunchIdViaCmdLine(kLaunchIdSettings, /*reply=*/""));

  // Validate the click values.
  EXPECT_EQ(NotificationCommon::OPERATION_SETTINGS, last_operation_);
  EXPECT_EQ(NotificationHandler::Type::WEB_PERSISTENT, last_notification_type_);
  EXPECT_EQ(GURL("https://example.com/"), last_origin_);
  EXPECT_EQ("notification_id", last_notification_id_);
  EXPECT_EQ(base::nullopt, last_action_index_);
  EXPECT_EQ(base::nullopt, last_reply_);
  EXPECT_TRUE(last_by_user_);
}
