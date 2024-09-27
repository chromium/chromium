// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <wrl/client.h>
#include <wrl/implements.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/metrics/statistics_recorder.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat_win.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/threading/thread_restrictions.h"
#include "base/win/scoped_hstring.h"
#include "base/win/shortcut.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_platform_bridge_win.h"
#include "chrome/browser/notifications/win/fake_itoastnotification.h"
#include "chrome/browser/notifications/win/fake_itoastnotifier.h"
#include "chrome/browser/notifications/win/notification_launch_id.h"
#include "chrome/browser/notifications/win/notification_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/shell_integration_win.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notifications/notification_operation.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_test.h"

namespace mswr = Microsoft::WRL;
namespace winui = ABI::Windows::UI;

namespace {

const char kLaunchId[] =
    "0|0|Default|aumi|0|https://example.com/|notification_id";
const char kLaunchIdButtonClick[] =
    "1|0|0|Default|aumi|0|https://example.com/|notification_id";
const char kLaunchIdSettings[] =
    "2|0|Default|aumi|0|https://example.com/|notification_id";

constexpr wchar_t kAppUserModelId[] = L"aumi";
constexpr wchar_t kAppUserModelId2[] = L"aumi2";

Profile* CreateTestingProfile(const base::FilePath& path) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  size_t starting_number_of_profiles = profile_manager->GetNumberOfProfiles();

  if (!base::PathExists(path) && !base::CreateDirectory(path))
    NOTREACHED_IN_MIGRATION()
        << "Could not create directory at " << path.MaybeAsASCII();

  std::unique_ptr<Profile> profile =
      Profile::CreateProfile(path, nullptr, Profile::CreateMode::kSynchronous);
  Profile* profile_ptr = profile.get();
  profile_manager->RegisterTestingProfile(std::move(profile), true);
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

std::wstring GetBrowserAppId() {
  return ShellUtil::GetBrowserModelId(InstallUtil::IsPerUserInstall());
}

static std::wstring GetWebAppIdForNotification(
    const webapps::AppId& web_app_id,
    const base::FilePath& profile_path) {
  return shell_integration::win::GetAppUserModelIdForApp(
      base::UTF8ToWide(web_app::GenerateApplicationNameFromAppId(web_app_id)),
      profile_path);
}

std::wstring GetToastString(const std::wstring& notification_id,
                            const std::wstring& profile_id,
                            const std::wstring& app_user_model_id,
                            bool incognito) {
  return base::StrCat({L"<toast launch=\"0|0|", profile_id, L"|",
                       app_user_model_id, L"|",
                       base::NumberToWString(incognito), L"|https://foo.com/|",
                       notification_id, L"\"></toast>"});
}

NotificationPlatformBridgeWin* GetBridge() {
  return static_cast<NotificationPlatformBridgeWin*>(
      g_browser_process->notification_platform_bridge());
}

void ValidateLaunchId(const std::string& expected_launch_id,
                      const base::RepeatingClosure& quit_task,
                      const NotificationLaunchId& launch_id) {
  ASSERT_TRUE(launch_id.is_valid());
  ASSERT_STREQ(expected_launch_id.c_str(), launch_id.Serialize().c_str());
  quit_task.Run();
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

  void TearDownOnMainThread() override { display_service_tester_.reset(); }

  void HandleOperation(const base::RepeatingClosure& quit_task,
                       NotificationOperation operation,
                       NotificationHandler::Type notification_type,
                       const GURL& origin,
                       const std::string& notification_id,
                       const std::optional<int>& action_index,
                       const std::optional<std::u16string>& reply,
                       const std::optional<bool>& by_user) {
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

  void OnHistogramRecorded(const base::RepeatingClosure& quit_closure,
                           const char* histogram_name,
                           uint64_t name_hash,
                           base::HistogramBase::Sample sample) {
    quit_closure.Run();
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

  bool ValidateNotificationValues(NotificationOperation operation,
                                  NotificationHandler::Type notification_type,
                                  const GURL& origin,
                                  const std::string& notification_id,
                                  const std::optional<int>& action_index,
                                  const std::optional<std::u16string>& reply,
                                  const std::optional<bool>& by_user) {
    return operation == last_operation_ &&
           notification_type == last_notification_type_ &&
           origin == last_origin_ && notification_id == last_notification_id_ &&
           action_index == last_action_index_ && reply == last_reply_ &&
           by_user == last_by_user_;
  }

  std::unique_ptr<NotificationDisplayServiceTester> display_service_tester_;

  NotificationOperation last_operation_;
  NotificationHandler::Type last_notification_type_;
  GURL last_origin_;
  std::string last_notification_id_;
  std::optional<int> last_action_index_;
  std::optional<std::u16string> last_reply_;
  std::optional<bool> last_by_user_;

  std::set<std::string> displayed_notifications_;

  bool delegate_called_ = false;
};

class FakeIToastActivatedEventArgs
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          winui::Notifications::IToastActivatedEventArgs> {
 public:
  explicit FakeIToastActivatedEventArgs(const std::wstring& args)
      : arguments_(args) {}
  FakeIToastActivatedEventArgs(const FakeIToastActivatedEventArgs&) = delete;
  FakeIToastActivatedEventArgs& operator=(const FakeIToastActivatedEventArgs&) =
      delete;
  ~FakeIToastActivatedEventArgs() override = default;

  HRESULT STDMETHODCALLTYPE get_Arguments(HSTRING* value) override {
    base::win::ScopedHString arguments =
        base::win::ScopedHString::Create(arguments_);
    *value = arguments.get();
    return S_OK;
  }

 private:
  std::wstring arguments_;
};

IN_PROC_BROWSER_TEST_F(NotificationPlatformBridgeWinUITest, HandleEvent) {
  const wchar_t kXmlDoc[] =
      LR"(<toast launch="0|0|Default|aumi|0|https://example.com/|notification_id">
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
      L"1|1|0|Default|aumi|0|https://example.com/|notification_id");

  base::RunLoop run_loop;
  display_service_tester_->SetProcessNotificationOperationDelegate(
      base::BindRepeating(&NotificationPlatformBridgeWinUITest::HandleOperation,
                          base::Unretained(this), run_loop.QuitClosure()));

  // Simulate clicks on the toast.
  NotificationPlatformBridgeWin* bridge = GetBridge();
  ASSERT_TRUE(bridge);
  bridge->ForwardHandleEventForTesting(NotificationOperation::kClick, &toast,
                                       &args, std::nullopt);
  run_loop.Run();

  // Validate the click values.
  EXPECT_EQ(NotificationOperation::kClick, last_operation_);
  EXPECT_EQ(NotificationHandler::Type::WEB_PERSISTENT, last_notification_type_);
  EXPECT_EQ(GURL("https://example.com/"), last_origin_);
  EXPECT_EQ("notification_id", last_notification_id_);
  EXPECT_EQ(1, last_action_index_);
  EXPECT_EQ(std::nullopt, last_reply_);
  EXPECT_EQ(std::nullopt, last_by_user_);
}

IN_PROC_BROWSER_TEST_F(NotificationPlatformBridgeWinUITest, HandleActivation) {
  base::RunLoop run_loop;
  display_service_tester_->SetProcessNotificationOperationDelegate(
      base::BindRepeating(&NotificationPlatformBridgeWinUITest::HandleOperation,
                          base::Unretained(this), run_loop.QuitClosure()));

  // Simulate notification activation.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchNative(
      switches::kNotificationLaunchId,
      L"1|1|0|Default|aumi|0|https://example.com/|notification_id");
  NotificationPlatformBridgeWin::HandleActivation(command_line);
  run_loop.Run();

  // Validate the values.
  EXPECT_EQ(NotificationOperation::kClick, last_operation_);
  EXPECT_EQ(NotificationHandler::Type::WEB_PERSISTENT, last_notification_type_);
  EXPECT_EQ(GURL("https://example.com/"), last_origin_);
  EXPECT_EQ("notification_id", last_notification_id_);
  EXPECT_EQ(1, last_action_index_);
  EXPECT_EQ(std::nullopt, last_reply_);
  EXPECT_EQ(true, last_by_user_);
}

IN_PROC_BROWSER_TEST_F(NotificationPlatformBridgeWinUITest, HandleSettings) {
  const wchar_t kXmlDoc[] =
      LR"(<toast launch="0|0|Default|aumi|0|https://example.com/|notification_id">
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
      L"2|0|Default|aumi|0|https://example.com/|notification_id");

  base::RunLoop run_loop;
  display_service_tester_->SetProcessNotificationOperationDelegate(
      base::BindRepeating(&NotificationPlatformBridgeWinUITest::HandleOperation,
                          base::Unretained(this), run_loop.QuitClosure()));

  // Simulate clicks on the toast.
  NotificationPlatformBridgeWin* bridge = GetBridge();
  ASSERT_TRUE(bridge);
  bridge->ForwardHandleEventForTesting(NotificationOperation::kSettings, &toast,
                                       &args, std::nullopt);
  run_loop.Run();

  // Validate the click values.
  EXPECT_EQ(NotificationOperation::kSettings, last_operation_);
  EXPECT_EQ(NotificationHandler::Type::WEB_PERSISTENT, last_notification_type_);
  EXPECT_EQ(GURL("https://example.com/"), last_origin_);
  EXPECT_EQ("notification_id", last_notification_id_);
  EXPECT_EQ(std::nullopt, last_action_index_);
  EXPECT_EQ(std::nullopt, last_reply_);
  EXPECT_EQ(std::nullopt, last_by_user_);
}

IN_PROC_BROWSER_TEST_F(NotificationPlatformBridgeWinUITest, HandleClose) {
  base::RunLoop run_loop;
  display_service_tester_->SetProcessNotificationOperationDelegate(
      base::BindRepeating(&NotificationPlatformBridgeWinUITest::HandleOperation,
                          base::Unretained(this), run_loop.QuitClosure()));

  // Simulate notification close.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchNative(
      switches::kNotificationLaunchId,
      L"3|0|Default|aumi|0|https://example.com/|notification_id");
  NotificationPlatformBridgeWin::HandleActivation(command_line);
  run_loop.Run();

  // Validate the values.
  EXPECT_EQ(NotificationOperation::kClose, last_operation_);
  EXPECT_EQ(NotificationHandler::Type::WEB_PERSISTENT, last_notification_type_);
  EXPECT_EQ(GURL("https://example.com/"), last_origin_);
  EXPECT_EQ("notification_id", last_notification_id_);
  EXPECT_EQ(std::nullopt, last_action_index_);
  EXPECT_EQ(std::nullopt, last_reply_);
  EXPECT_EQ(true, last_by_user_);
}

IN_PROC_BROWSER_TEST_F(NotificationPlatformBridgeWinUITest, GetDisplayed) {
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
        base::BindOnce(
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
      GetToastString(L"P1i", L"P1", kAppUserModelId, incognito), L"tag"));
  notifications.push_back(Microsoft::WRL::Make<FakeIToastNotification>(
      GetToastString(L"P1reg", L"P1", kAppUserModelId, !incognito), L"tag"));

  Profile* profile2 = CreateTestingProfile("P2");
  notifications.push_back(Microsoft::WRL::Make<FakeIToastNotification>(
      GetToastString(L"P2i", L"P2", kAppUserModelId, incognito), L"tag"));
  notifications.push_back(Microsoft::WRL::Make<FakeIToastNotification>(
      GetToastString(L"P2reg", L"P2", kAppUserModelId, !incognito), L"tag"));

  // Query for profile P1 in incognito (should return 1 item).
  {
    base::RunLoop run_loop;
    bridge->GetDisplayed(
        profile1->GetPrimaryOTRProfile(/*create_if_needed=*/true),
        base::BindOnce(
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
        base::BindOnce(
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
        profile2->GetPrimaryOTRProfile(/*create_if_needed=*/true),
        base::BindOnce(
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
        base::BindOnce(
            &NotificationPlatformBridgeWinUITest::DisplayedNotifications,
            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_EQ(1U, displayed_notifications_.size());
    EXPECT_EQ(1U, displayed_notifications_.count("P2reg"));
  }

  bridge->SetDisplayedNotificationsForTesting(nullptr);
}

IN_PROC_BROWSER_TEST_F(NotificationPlatformBridgeWinUITest,
                       SynchronizeNotifications) {
  NotificationPlatformBridgeWin* bridge = GetBridge();
  ASSERT_TRUE(bridge);

  std::map<NotificationPlatformBridgeWin::NotificationKeyType,
           NotificationLaunchId>
      expected_displayed_notifications;
  bridge->SetExpectedDisplayedNotificationsForTesting(
      &expected_displayed_notifications);

  std::vector<mswr::ComPtr<winui::Notifications::IToastNotification>>
      notifications;
  bridge->SetDisplayedNotificationsForTesting(&notifications);

  notifications.push_back(Microsoft::WRL::Make<FakeIToastNotification>(
      GetToastString(L"P1i", L"Default", kAppUserModelId, true), L"tag"));
  expected_displayed_notifications[{
      /*profile_id=*/"Default",
      /*notification_id=*/"P1i",
      /*app_user_model_id=*/kAppUserModelId,
  }] = GetNotificationLaunchId(notifications.back().Get());

  expected_displayed_notifications[{
      /*profile_id=*/"Default",
      /*notification_id=*/"P2i",
      /*app_user_model_id=*/kAppUserModelId2,
  }] =
      GetNotificationLaunchId(
          Microsoft::WRL::Make<FakeIToastNotification>(
              GetToastString(L"P2i", L"Default", kAppUserModelId2, false),
              L"tag")
              .Get());

  base::RunLoop run_loop;
  display_service_tester_->SetProcessNotificationOperationDelegate(
      base::BindRepeating(&NotificationPlatformBridgeWinUITest::HandleOperation,
                          base::Unretained(this), run_loop.QuitClosure()));
  // Simulate notifications synchronization.
  bridge->SynchronizeNotificationsForTesting();
  run_loop.Run();

  std::map<NotificationPlatformBridgeWin::NotificationKeyType,
           NotificationLaunchId>
      actual_expected_displayed_notification =
          bridge->GetExpectedDisplayedNotificationForTesting();

  // Only one notification is displayed (P1i). As result, the synchronization
  // will close the notification P2i.
  ASSERT_EQ(1u, actual_expected_displayed_notification.size());
  EXPECT_TRUE(actual_expected_displayed_notification.count({
      /*profile_id=*/"Default",
      /*notification_id=*/"P1i",
      /*app_user_model_id=*/kAppUserModelId,
  }));

  // Validate the close event values.
  EXPECT_EQ(NotificationOperation::kClose, last_operation_);
  EXPECT_EQ("P2i", last_notification_id_);
  EXPECT_EQ(std::nullopt, last_action_index_);
  EXPECT_EQ(std::nullopt, last_reply_);
  EXPECT_EQ(true, last_by_user_);

  bridge->SetDisplayedNotificationsForTesting(nullptr);
  bridge->SetExpectedDisplayedNotificationsForTesting(nullptr);
}

IN_PROC_BROWSER_TEST_F(NotificationPlatformBridgeWinUITest,
                       SynchronizeNotificationsAfterClose) {
  NotificationPlatformBridgeWin* bridge = GetBridge();
  ASSERT_TRUE(bridge);
  FakeIToastNotifier notifier;
  bridge->SetNotifierForTesting(&notifier);

  // Show a new notification.
  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, "notification_id", u"Text1",
      u"Text2", ui::ImageModel(), std::u16string(),
      GURL("https://example.com/"), message_center::NotifierId(),
      message_center::RichNotificationData(), nullptr);
  base::RunLoop display_run_loop;
  base::StatisticsRecorder::ScopedHistogramSampleObserver
      display_histogram_observer(
          "Notifications.Windows.DisplayStatus",
          base::BindRepeating(
              &NotificationPlatformBridgeWinUITest::OnHistogramRecorded,
              base::Unretained(this), display_run_loop.QuitClosure()));
  bridge->Display(NotificationHandler::Type::WEB_PERSISTENT,
                  browser()->profile(), notification, /*metadata=*/nullptr);
  display_run_loop.Run();

  // The notification should now be in the expected map.
  EXPECT_EQ(1u, bridge->GetExpectedDisplayedNotificationForTesting().size());

  // Close the notification
  base::RunLoop close_run_loop;
  base::StatisticsRecorder::ScopedHistogramSampleObserver
      close_histogram_observer(
          "Notifications.Windows.CloseStatus",
          base::BindRepeating(
              &NotificationPlatformBridgeWinUITest::OnHistogramRecorded,
              base::Unretained(this), close_run_loop.QuitClosure()));
  bridge->Close(browser()->profile(), notification.id());
  close_run_loop.Run();

  // Closing a notification should remove it from the expected map.
  EXPECT_EQ(0u, bridge->GetExpectedDisplayedNotificationForTesting().size());

  bridge->SetNotifierForTesting(nullptr);
}

// Test calling Display with a fake implementation of the Action Center
// and validate it gets the values expected.
IN_PROC_BROWSER_TEST_F(NotificationPlatformBridgeWinUITest, DisplayWithFakeAC) {
  NotificationPlatformBridgeWin* bridge = GetBridge();
  ASSERT_TRUE(bridge);

  FakeIToastNotifier notifier;
  bridge->SetNotifierForTesting(&notifier);

  std::string launch_id_value = "0|0|P1|" +
                                base::WideToUTF8(GetBrowserAppId().c_str()) +
                                "|0|https://example.com/|notification_id";
  NotificationLaunchId launch_id(launch_id_value);
  ASSERT_TRUE(launch_id.is_valid());

  auto notification = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE, "notification_id", u"Text1",
      u"Text2", ui::ImageModel(), std::u16string(),
      GURL("https://example.com/"), message_center::NotifierId(),
      message_center::RichNotificationData(), nullptr);

  std::unique_ptr<NotificationCommon::Metadata> metadata;
  Profile* profile = CreateTestingProfile("P1");

  {
    base::RunLoop run_loop;
    notifier.SetNotificationShownCallback(base::BindRepeating(
        &ValidateLaunchId, launch_id_value, run_loop.QuitClosure()));
    bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile,
                    *notification, std::move(metadata));
    run_loop.Run();
  }

  bridge->SetNotifierForTesting(nullptr);
}

// Test calling Display for a web app notification with a fake implementation of
// the Action Center and validate it gets the values expected.
// If the app is not in the start menu, a normal Chrome notification is shown.
// If the kAppSpecificNotifications experiment is enabled, and the app is in the
// start menu, a web-app specific notification should be shown.
IN_PROC_BROWSER_TEST_F(NotificationPlatformBridgeWinUITest,
                       DisplayWebAppNotificationWithFakeAC) {
  NotificationPlatformBridgeWin* bridge = GetBridge();
  ASSERT_TRUE(bridge);

  FakeIToastNotifier notifier;
  bridge->SetNotifierForTesting(&notifier);

  auto notification = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE, "notification_id", u"Text1",
      u"Text2", ui::ImageModel(), std::u16string(),
      GURL("https://example.com/"),
      message_center::NotifierId(GURL("https://example.com/"), u"webpagetitle",
                                 base::WideToUTF8(GetBrowserAppId().c_str())),
      message_center::RichNotificationData(), nullptr);

  std::string launch_id_value =
      base::StrCat({"0|0|P1|", base::WideToUTF8(GetBrowserAppId().c_str()),
                    "|0|https://example.com/|notification_id"});
  NotificationLaunchId launch_id(launch_id_value);
  ASSERT_TRUE(launch_id.is_valid());

  std::unique_ptr<NotificationCommon::Metadata> metadata;
  Profile* profile = CreateTestingProfile("P1");
  {
    base::RunLoop run_loop;
    notifier.SetNotificationShownCallback(base::BindRepeating(
        &ValidateLaunchId, launch_id_value, run_loop.QuitClosure()));
    bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile,
                    *notification, std::move(metadata));
    run_loop.Run();
  }
  bridge->SetNotifierForTesting(nullptr);
}

class NotificationPlatformBridgeWinAppInstalledTest
    : public web_app::WebAppBrowserTestBase {
 public:
  NotificationPlatformBridgeWinAppInstalledTest()
      : web_app::WebAppBrowserTestBase({features::kAppSpecificNotifications},
                                       {}) {}
  void SetUpOnMainThread() override {
    webapps::AppId pwa_app_id = InstallPWA(GURL("https://example.com/"));

    notification_ = std::make_unique<message_center::Notification>(
        message_center::NOTIFICATION_TYPE_SIMPLE, "notification_id", u"Text1",
        u"Text2", ui::ImageModel(), std::u16string(),
        GURL("https://example.com/"),
        message_center::NotifierId(GURL("https://example.com/"),
                                   u"webpagetitle", pwa_app_id.c_str()),
        message_center::RichNotificationData(), nullptr);

    web_app::WebAppBrowserTestBase::SetUpOnMainThread();
  }

  void SetNotifier(NotificationPlatformBridgeWin* bridge,
                   ABI::Windows::UI::Notifications::IToastNotifier* notifier) {
    bridge->SetNotifierForTesting(notifier);
  }

  base::FilePath GetStartMenuShortcutPath(Profile* profile) {
    std::string web_app_id =
        notification_->notifier_id().web_app_id.value_or("");
    std::string app_name;
    CHECK(!web_app_id.empty());
    auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
    if (provider) {
      app_name = provider->registrar_unsafe().GetAppShortName(web_app_id);
    }

    std::wstring file_name = base::AsWString(base::UTF8ToUTF16(app_name));

    base::FilePath chrome_apps_dir;
    CHECK(ShellUtil::GetShortcutPath(
        ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_APPS_DIR,
        ShellUtil::CURRENT_USER, &chrome_apps_dir));

    return chrome_apps_dir.Append(file_name).AddExtension(installer::kLnkExt);
  }

  void DisplayNotificationAndCheckLaunchId(Profile* profile,
                                           const std::string& app_id) {
    NotificationPlatformBridgeWin* bridge = GetBridge();
    ASSERT_TRUE(bridge);

    FakeIToastNotifier notifier;
    SetNotifier(bridge, &notifier);

    std::string expected_launch_id = base::StrCat(
        {"0|0|", NotificationPlatformBridge::GetProfileId(profile), "|",
         app_id.c_str(), "|0|https://example.com/|notification_id"});
    NotificationLaunchId launch_id(expected_launch_id);
    ASSERT_TRUE(launch_id.is_valid());

    std::unique_ptr<NotificationCommon::Metadata> metadata;

    {
      base::RunLoop run_loop;
      notifier.SetNotificationShownCallback(base::BindRepeating(
          &ValidateLaunchId, expected_launch_id, run_loop.QuitClosure()));
      bridge->Display(NotificationHandler::Type::WEB_PERSISTENT, profile,
                      *notification_, std::move(metadata));
      run_loop.Run();
    }
    SetNotifier(bridge, nullptr);
  }

 protected:
  std::unique_ptr<message_center::Notification> notification_;
};

IN_PROC_BROWSER_TEST_F(NotificationPlatformBridgeWinAppInstalledTest,
                       DisplayWebAppNotification) {
  Profile* profile = WebAppBrowserTestBase::profile();
  std::wstring app_id = GetWebAppIdForNotification(
      notification_->notifier_id().web_app_id.value_or(""), profile->GetPath());

  DisplayNotificationAndCheckLaunchId(profile, base::WideToUTF8(app_id));
}

// Tests that a Chrome notification is shown if the PWA doesn't have a shortcut
// in the Chrome Apps sub-menu of the start menu.
IN_PROC_BROWSER_TEST_F(NotificationPlatformBridgeWinAppInstalledTest,
                       DisplayChromeNotificationNoStartMenuShortcut) {
  Profile* profile = WebAppBrowserTestBase::profile();

  base::FilePath start_menu_shortcut_path = GetStartMenuShortcutPath(profile);
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::DeleteFile(start_menu_shortcut_path));
  }

  DisplayNotificationAndCheckLaunchId(profile,
                                      base::WideToUTF8(GetBrowserAppId()));
}

// Test that a Chrome notification is shown if the Web App shortcut in the
// start menu has the wrong AUMI.
IN_PROC_BROWSER_TEST_F(NotificationPlatformBridgeWinAppInstalledTest,
                       DisplayChromeNotificationStartMenuShortcutWrongAUMI) {
  Profile* profile = WebAppBrowserTestBase::profile();

  base::FilePath start_menu_shortcut_path = GetStartMenuShortcutPath(profile);
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    // Change the AUMI on the start menu shortcut.
    base::win::ShortcutProperties new_properties;
    new_properties.set_app_id(L"wrongAUMI");
    ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
        start_menu_shortcut_path, new_properties,
        base::win::ShortcutOperation::kUpdateExisting));
  }
  DisplayNotificationAndCheckLaunchId(
      profile, base::WideToUTF8(GetBrowserAppId().c_str()));
}

IN_PROC_BROWSER_TEST_F(NotificationPlatformBridgeWinUITest, CmdLineClick) {
  ASSERT_NO_FATAL_FAILURE(ProcessLaunchIdViaCmdLine(kLaunchId, /*reply=*/""));

  // Validate the click values.
  EXPECT_EQ(NotificationOperation::kClick, last_operation_);
  EXPECT_EQ(NotificationHandler::Type::WEB_PERSISTENT, last_notification_type_);
  EXPECT_EQ(GURL("https://example.com/"), last_origin_);
  EXPECT_EQ("notification_id", last_notification_id_);
  EXPECT_EQ(std::nullopt, last_action_index_);
  EXPECT_EQ(std::nullopt, last_reply_);
  EXPECT_TRUE(last_by_user_);
}

IN_PROC_BROWSER_TEST_F(NotificationPlatformBridgeWinUITest,
                       CmdLineInlineReply) {
  ASSERT_NO_FATAL_FAILURE(
      ProcessLaunchIdViaCmdLine(kLaunchIdButtonClick, "Inline reply"));

  // Validate the click values.
  EXPECT_EQ(NotificationOperation::kClick, last_operation_);
  EXPECT_EQ(NotificationHandler::Type::WEB_PERSISTENT, last_notification_type_);
  EXPECT_EQ(GURL("https://example.com/"), last_origin_);
  EXPECT_EQ("notification_id", last_notification_id_);
  EXPECT_EQ(0, last_action_index_);
  EXPECT_EQ(u"Inline reply", last_reply_);
  EXPECT_TRUE(last_by_user_);
}

IN_PROC_BROWSER_TEST_F(NotificationPlatformBridgeWinUITest, CmdLineButton) {
  ASSERT_NO_FATAL_FAILURE(
      ProcessLaunchIdViaCmdLine(kLaunchIdButtonClick, /*reply=*/""));

  // Validate the click values.
  EXPECT_EQ(NotificationOperation::kClick, last_operation_);
  EXPECT_EQ(NotificationHandler::Type::WEB_PERSISTENT, last_notification_type_);
  EXPECT_EQ(GURL("https://example.com/"), last_origin_);
  EXPECT_EQ("notification_id", last_notification_id_);
  EXPECT_EQ(0, last_action_index_);
  EXPECT_EQ(std::nullopt, last_reply_);
  EXPECT_TRUE(last_by_user_);
}

IN_PROC_BROWSER_TEST_F(NotificationPlatformBridgeWinUITest, CmdLineSettings) {
  ASSERT_NO_FATAL_FAILURE(
      ProcessLaunchIdViaCmdLine(kLaunchIdSettings, /*reply=*/""));

  // Validate the click values.
  EXPECT_EQ(NotificationOperation::kSettings, last_operation_);
  EXPECT_EQ(NotificationHandler::Type::WEB_PERSISTENT, last_notification_type_);
  EXPECT_EQ(GURL("https://example.com/"), last_origin_);
  EXPECT_EQ("notification_id", last_notification_id_);
  EXPECT_EQ(std::nullopt, last_action_index_);
  EXPECT_EQ(std::nullopt, last_reply_);
  EXPECT_TRUE(last_by_user_);
}
