// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_linux.h"

#include <dbus/dbus-shared.h>

#include <memory>
#include <vector>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/number_formatting.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/common/notifications/notification_operation.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "content/public/test/test_utils.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

using message_center::ButtonInfo;
using message_center::Notification;
using message_center::SettingsButtonHandler;
using testing::_;
using testing::ByMove;
using testing::Return;
using testing::StrictMock;

namespace {

const char kFreedesktopNotificationsName[] = "org.freedesktop.Notifications";
const char kFreedesktopNotificationsPath[] = "/org/freedesktop/Notifications";

class NotificationBuilder {
 public:
  explicit NotificationBuilder(const std::string& id)
      : notification_(message_center::NOTIFICATION_TYPE_SIMPLE,
                      id,
                      std::u16string(),
                      std::u16string(),
                      ui::ImageModel(),
                      std::u16string(),
                      GURL(),
                      message_center::NotifierId(GURL()),
                      message_center::RichNotificationData(),
                      new message_center::NotificationDelegate()) {
    notification_.set_settings_button_handler(SettingsButtonHandler::DELEGATE);
  }

  Notification GetResult() { return notification_; }

  NotificationBuilder& SetImage(const gfx::Image& image) {
    notification_.SetImage(image);
    return *this;
  }

  NotificationBuilder& SetItems(
      const std::vector<message_center::NotificationItem>& items) {
    notification_.set_items(items);
    return *this;
  }

  NotificationBuilder& SetMessage(const std::u16string& message) {
    notification_.set_message(message);
    return *this;
  }

  NotificationBuilder& SetNeverTimeout(bool never_timeout) {
    notification_.set_never_timeout(never_timeout);
    return *this;
  }

  NotificationBuilder& SetOriginUrl(const GURL& origin_url) {
    notification_.set_origin_url(origin_url);
    return *this;
  }

  NotificationBuilder& SetProgress(int progress) {
    notification_.set_progress(progress);
    return *this;
  }

  NotificationBuilder& SetSettingsButtonHandler(SettingsButtonHandler handler) {
    notification_.set_settings_button_handler(handler);
    return *this;
  }

  NotificationBuilder& SetSilent(bool silent) {
    notification_.set_silent(silent);
    return *this;
  }

  NotificationBuilder& SetTitle(const std::u16string& title) {
    notification_.set_title(title);
    return *this;
  }

  NotificationBuilder& SetType(message_center::NotificationType type) {
    notification_.set_type(type);
    return *this;
  }

  NotificationBuilder& AddButton(const ButtonInfo& button) {
    auto buttons = notification_.buttons();
    buttons.push_back(button);
    notification_.set_buttons(buttons);
    return *this;
  }

 private:
  Notification notification_;
};

struct NotificationRequest {
  struct Action {
    std::string id;
    std::string label;
  };

  std::string summary;
  std::string body;
  std::string kde_origin_name;
  std::vector<Action> actions;
  int32_t expire_timeout = 0;
  bool silent = false;
};

struct TestParams {
  TestParams()
      : name_has_owner(true),
        capabilities{"actions", "body", "body-hyperlinks", "body-images",
                     "body-markup"},
        server_name("NPBL_unittest"),
        server_version("1.0"),
        expect_init_success(true),
        expect_shutdown(true),
        connect_signals(true) {}

  TestParams& SetNameHasOwner(bool new_name_has_owner) {
    this->name_has_owner = new_name_has_owner;
    return *this;
  }

  TestParams& SetCapabilities(
      const std::vector<std::string>& new_capabilities) {
    this->capabilities = new_capabilities;
    return *this;
  }

  TestParams& SetServerName(const std::string& new_server_name) {
    this->server_name = new_server_name;
    return *this;
  }

  TestParams& SetServerVersion(const std::string& new_server_version) {
    this->server_version = new_server_version;
    return *this;
  }

  TestParams& SetExpectInitSuccess(bool new_expect_init_success) {
    this->expect_init_success = new_expect_init_success;
    return *this;
  }

  TestParams& SetExpectShutdown(bool new_expect_shutdown) {
    this->expect_shutdown = new_expect_shutdown;
    return *this;
  }

  TestParams& SetConnectSignals(bool new_connect_signals) {
    this->connect_signals = new_connect_signals;
    return *this;
  }

  bool name_has_owner;
  std::vector<std::string> capabilities;
  std::string server_name;
  std::string server_version;
  bool expect_init_success;
  bool expect_shutdown;
  bool connect_signals;
};

NotificationRequest ParseRequest(dbus::MethodCall* method_call) {
  // The "Notify" message must have type (susssasa{sv}i).
  // https://developer.gnome.org/notification-spec/#command-notify
  NotificationRequest request;

  dbus::MessageReader reader(method_call);
  std::string str;
  uint32_t uint32;
  EXPECT_TRUE(reader.PopString(&str));              // app_name
  EXPECT_TRUE(reader.PopUint32(&uint32));           // replaces_id
  EXPECT_TRUE(reader.PopString(&str));              // app_icon
  EXPECT_TRUE(reader.PopString(&request.summary));  // summary
  EXPECT_TRUE(reader.PopString(&request.body));     // body

  {
    dbus::MessageReader actions_reader(nullptr);
    EXPECT_TRUE(reader.PopArray(&actions_reader));
    while (actions_reader.HasMoreData()) {
      // Actions come in pairs.
      std::string id;
      std::string label;
      EXPECT_TRUE(actions_reader.PopString(&id));
      EXPECT_TRUE(actions_reader.PopString(&label));
      request.actions.push_back({id, label});
    }
  }

  {
    dbus::MessageReader hints_reader(nullptr);
    EXPECT_TRUE(reader.PopArray(&hints_reader));
    while (hints_reader.HasMoreData()) {
      dbus::MessageReader dict_entry_reader(nullptr);
      EXPECT_TRUE(hints_reader.PopDictEntry(&dict_entry_reader));
      EXPECT_TRUE(dict_entry_reader.PopString(&str));
      dbus::MessageReader variant_reader(nullptr);
      if (str == "suppress-sound") {
        bool suppress_sound;
        EXPECT_TRUE(dict_entry_reader.PopVariantOfBool(&suppress_sound));
        request.silent = suppress_sound;
      } else if (str == "x-kde-origin-name") {
        std::string x_kde_origin_name;
        EXPECT_TRUE(dict_entry_reader.PopVariantOfString(&x_kde_origin_name));
        request.kde_origin_name = x_kde_origin_name;
      } else {
        EXPECT_TRUE(dict_entry_reader.PopVariant(&variant_reader));
      }
      EXPECT_FALSE(dict_entry_reader.HasMoreData());
    }
  }

  EXPECT_TRUE(reader.PopInt32(&request.expire_timeout));
  EXPECT_FALSE(reader.HasMoreData());

  return request;
}

std::unique_ptr<dbus::Response> GetIdResponse(uint32_t id) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(response.get());
  writer.AppendUint32(id);
  return response;
}

ACTION_P2(OnGetServerInformation, server_name, server_version) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(response.get());
  writer.AppendString(server_name);     // name
  writer.AppendString("chromium");      // vendor
  writer.AppendString(server_version);  // version
  writer.AppendString("1.2");           // spec_version
  return response;
}

ACTION_P(RegisterSignalCallback, callback_addr) {
  *callback_addr = arg2;
  std::move(*arg3).Run("" /* interface_name */, "" /* signal_name */,
                       true /* success */);
}

ACTION_P2(OnNotify, verifier, id) {
  verifier(ParseRequest(arg0));
  return GetIdResponse(id);
}

ACTION(OnCloseNotification) {
  // The "CloseNotification" message must have type (u).
  // https://developer.gnome.org/notification-spec/#command-close-notification
  dbus::MethodCall* method_call = arg0;
  dbus::MessageReader reader(method_call);
  uint32_t uint32;
  EXPECT_TRUE(reader.PopUint32(&uint32));
  EXPECT_FALSE(reader.HasMoreData());

  return dbus::Response::CreateEmpty();
}

ACTION_P(OnNotificationBridgeReady, success) {
  EXPECT_EQ(success, arg0);
}

// Matches a method call to the specified dbus target.
MATCHER_P(Calls, member, "") {
  return arg->GetMember() == member;
}

}  // namespace

class NotificationPlatformBridgeLinuxTest : public BrowserWithTestWindowTest {
 public:
  NotificationPlatformBridgeLinuxTest() = default;
  NotificationPlatformBridgeLinuxTest(
      const NotificationPlatformBridgeLinuxTest&) = delete;
  NotificationPlatformBridgeLinuxTest& operator=(
      const NotificationPlatformBridgeLinuxTest&) = delete;
  ~NotificationPlatformBridgeLinuxTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(profile());
    display_service_tester_->SetProcessNotificationOperationDelegate(
        base::BindRepeating(
            &NotificationPlatformBridgeLinuxTest::HandleOperation,
            base::Unretained(this)));
    mock_bus_ = base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());
    mock_dbus_proxy_ = base::MakeRefCounted<StrictMock<dbus::MockObjectProxy>>(
        mock_bus_.get(), DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));
    mock_notification_proxy_ =
        base::MakeRefCounted<StrictMock<dbus::MockObjectProxy>>(
            mock_bus_.get(), kFreedesktopNotificationsName,
            dbus::ObjectPath(kFreedesktopNotificationsPath));
  }

  void TearDown() override {
    notification_bridge_linux_->CleanUp();
    content::RunAllTasksUntilIdle();
    notification_bridge_linux_.reset();
    display_service_tester_.reset();
    mock_dbus_proxy_.reset();
    mock_notification_proxy_ = nullptr;
    mock_bus_ = nullptr;
    BrowserWithTestWindowTest::TearDown();
  }

  void HandleOperation(NotificationOperation operation,
                       NotificationHandler::Type notification_type,
                       const GURL& origin,
                       const std::string& notification_id,
                       const std::optional<int>& action_index,
                       const std::optional<std::u16string>& reply,
                       const std::optional<bool>& by_user) {
    last_operation_ = operation;
    last_action_index_ = action_index;
    last_reply_ = reply;
  }

 protected:
  void CreateNotificationBridgeLinux(const TestParams& test_params) {
    EXPECT_CALL(
        *mock_bus_.get(),
        GetObjectProxy(DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS)))
        .WillOnce(Return(mock_dbus_proxy_.get()));
    EXPECT_CALL(*mock_bus_.get(),
                GetObjectProxy(kFreedesktopNotificationsName,
                               dbus::ObjectPath(kFreedesktopNotificationsPath)))
        .WillOnce(Return(mock_notification_proxy_.get()));

    auto name_has_owner_response = dbus::Response::CreateEmpty();
    dbus::MessageWriter name_has_owner_writer(name_has_owner_response.get());
    name_has_owner_writer.AppendBool(test_params.name_has_owner);
    EXPECT_CALL(*mock_dbus_proxy_.get(),
                CallMethodAndBlock(Calls("NameHasOwner"), _))
        .WillOnce(Return(ByMove(std::move(name_has_owner_response))));

    auto capabilities_response = dbus::Response::CreateEmpty();
    dbus::MessageWriter capabilities_writer(capabilities_response.get());
    capabilities_writer.AppendArrayOfStrings(test_params.capabilities);
    EXPECT_CALL(*mock_notification_proxy_.get(),
                CallMethodAndBlock(Calls("GetCapabilities"), _))
        .WillOnce(Return(ByMove(std::move(capabilities_response))));

    if (test_params.expect_init_success) {
      EXPECT_CALL(*mock_notification_proxy_.get(),
                  CallMethodAndBlock(Calls("GetServerInformation"), _))
          .WillOnce(OnGetServerInformation(test_params.server_name,
                                           test_params.server_version));
    }

    if (test_params.connect_signals) {
      EXPECT_CALL(*mock_notification_proxy_.get(),
                  DoConnectToSignal(kFreedesktopNotificationsName,
                                    "ActionInvoked", _, _))
          .WillOnce(RegisterSignalCallback(&action_invoked_callback_));

      EXPECT_CALL(*mock_notification_proxy_.get(),
                  DoConnectToSignal(kFreedesktopNotificationsName,
                                    "NotificationClosed", _, _))
          .WillOnce(RegisterSignalCallback(&notification_closed_callback_));

      EXPECT_CALL(*mock_notification_proxy_.get(),
                  DoConnectToSignal(kFreedesktopNotificationsName,
                                    "NotificationReplied", _, _))
          .WillOnce(RegisterSignalCallback(&notification_replied_callback_));
    }

    EXPECT_CALL(*this, MockableNotificationBridgeReadyCallback(_))
        .WillOnce(OnNotificationBridgeReady(test_params.expect_init_success));

    if (test_params.expect_shutdown)
      EXPECT_CALL(*mock_bus_.get(), ShutdownAndBlock());

    notification_bridge_linux_ =
        base::WrapUnique(new NotificationPlatformBridgeLinux(mock_bus_));
    notification_bridge_linux_->SetReadyCallback(
        base::BindOnce(&NotificationPlatformBridgeLinuxTest::
                           MockableNotificationBridgeReadyCallback,
                       base::Unretained(this)));
    content::RunAllTasksUntilIdle();
  }

  void InvokeAction(uint32_t dbus_id, const std::string& action) {
    dbus_thread_linux::GetTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&NotificationPlatformBridgeLinuxTest::DoInvokeAction,
                       base::Unretained(this), dbus_id, action));
  }

  void ReplyToNotification(uint32_t dbus_id, const std::string& message) {
    dbus_thread_linux::GetTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &NotificationPlatformBridgeLinuxTest::DoReplyToNotification,
            base::Unretained(this), dbus_id, message));
  }

  MOCK_METHOD1(MockableNotificationBridgeReadyCallback, void(bool));

  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_dbus_proxy_;
  scoped_refptr<dbus::MockObjectProxy> mock_notification_proxy_;

  base::OnceCallback<void(dbus::Signal*)> action_invoked_callback_;
  base::OnceCallback<void(dbus::Signal*)> notification_closed_callback_;
  base::OnceCallback<void(dbus::Signal*)> notification_replied_callback_;

  std::unique_ptr<NotificationPlatformBridgeLinux> notification_bridge_linux_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_tester_;

  std::optional<NotificationOperation> last_operation_;
  std::optional<int> last_action_index_;
  std::optional<std::u16string> last_reply_;

 private:
  void DoInvokeAction(uint32_t dbus_id, const std::string& action) {
    dbus::Signal signal(kFreedesktopNotificationsName, "ActionInvoked");
    dbus::MessageWriter writer(&signal);
    writer.AppendUint32(dbus_id);
    writer.AppendString(action);
    std::move(action_invoked_callback_).Run(&signal);
  }

  void DoReplyToNotification(uint32_t dbus_id, const std::string& message) {
    dbus::Signal signal(kFreedesktopNotificationsName, "NotificationReplied");
    dbus::MessageWriter writer(&signal);
    writer.AppendUint32(dbus_id);
    writer.AppendString(message);
    std::move(notification_replied_callback_).Run(&signal);
  }
};

TEST_F(NotificationPlatformBridgeLinuxTest, SetUpAndTearDown) {
  CreateNotificationBridgeLinux(TestParams());
}

TEST_F(NotificationPlatformBridgeLinuxTest, NotifyAndCloseFormat) {
  EXPECT_CALL(*mock_notification_proxy_.get(),
              CallMethodAndBlock(Calls("Notify"), _))
      .WillOnce(OnNotify([](const NotificationRequest&) {}, 1));
  EXPECT_CALL(*mock_notification_proxy_.get(),
              CallMethodAndBlock(Calls("CloseNotification"), _))
      .WillOnce(OnCloseNotification());

  CreateNotificationBridgeLinux(TestParams());
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("").GetResult(), nullptr);
  notification_bridge_linux_->Close(profile(), "");
}

TEST_F(NotificationPlatformBridgeLinuxTest, ProgressPercentageAddedToSummary) {
  EXPECT_CALL(*mock_notification_proxy_.get(),
              CallMethodAndBlock(Calls("Notify"), _))
      .WillOnce(OnNotify(
          [](const NotificationRequest& request) {
            EXPECT_EQ(
                base::UTF16ToUTF8(base::FormatPercent(42)) + " - The Title",
                request.summary);
          },
          1));

  CreateNotificationBridgeLinux(TestParams());
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("")
          .SetType(message_center::NOTIFICATION_TYPE_PROGRESS)
          .SetProgress(42)
          .SetTitle(u"The Title")
          .GetResult(),
      nullptr);
}

TEST_F(NotificationPlatformBridgeLinuxTest, NotificationListItemsInBody) {
  EXPECT_CALL(*mock_notification_proxy_.get(),
              CallMethodAndBlock(Calls("Notify"), _))
      .WillOnce(OnNotify(
          [](const NotificationRequest& request) {
            EXPECT_EQ("<b>abc</b> 123\n<b>def</b> 456", request.body);
          },
          1));

  CreateNotificationBridgeLinux(TestParams());
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("")
          .SetType(message_center::NOTIFICATION_TYPE_MULTIPLE)
          .SetItems(std::vector<message_center::NotificationItem>{
              {u"abc", u"123"}, {u"def", u"456"}})
          .GetResult(),
      nullptr);
}

TEST_F(NotificationPlatformBridgeLinuxTest, NotificationTimeoutsNoPersistence) {
  const int32_t kExpireTimeout = 25000;
  const int32_t kExpireTimeoutNever = 0;
  EXPECT_CALL(*mock_notification_proxy_.get(),
              CallMethodAndBlock(Calls("Notify"), _))
      .WillOnce(OnNotify(
          [=](const NotificationRequest& request) {
            EXPECT_EQ(kExpireTimeout, request.expire_timeout);
          },
          1))
      .WillOnce(OnNotify(
          [=](const NotificationRequest& request) {
            EXPECT_EQ(kExpireTimeoutNever, request.expire_timeout);
          },
          2));

  CreateNotificationBridgeLinux(TestParams());
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("1").SetNeverTimeout(false).GetResult(), nullptr);
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("2").SetNeverTimeout(true).GetResult(), nullptr);
}

TEST_F(NotificationPlatformBridgeLinuxTest,
       NotificationTimeoutWithPersistence) {
  const int32_t kExpireTimeoutDefault = -1;
  EXPECT_CALL(*mock_notification_proxy_.get(),
              CallMethodAndBlock(Calls("Notify"), _))
      .WillOnce(OnNotify(
          [=](const NotificationRequest& request) {
            EXPECT_EQ(kExpireTimeoutDefault, request.expire_timeout);
          },
          1));

  CreateNotificationBridgeLinux(TestParams().SetCapabilities(
      std::vector<std::string>{"actions", "body", "persistence"}));
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("1").GetResult(), nullptr);
}

TEST_F(NotificationPlatformBridgeLinuxTest, NotificationImages) {
  const int kMaxImageWidth = 200;
  const int kMaxImageHeight = 100;

  const int original_width = kMaxImageWidth * 2;
  const int original_height = kMaxImageHeight;
  const int expected_width = kMaxImageWidth;
  const int expected_height = kMaxImageHeight / 2;

  gfx::Image original_image =
      gfx::Image(gfx::test::CreateImageSkia(original_width, original_height));

  EXPECT_CALL(*mock_notification_proxy_.get(),
              CallMethodAndBlock(Calls("Notify"), _))
      .WillOnce(OnNotify(
          [=](const NotificationRequest& request) {
            std::string file_name;
            EXPECT_TRUE(RE2::FullMatch(
                request.body,
                "\\<img src=\\\"file://(.+)\\\" alt=\\\".*\\\"/\\>",
                &file_name));
            std::string file_contents;
            EXPECT_TRUE(base::ReadFileToString(base::FilePath(file_name),
                                               &file_contents));
            gfx::Image image = gfx::Image::CreateFrom1xPNGBytes(
                base::as_byte_span(file_contents));
            EXPECT_EQ(expected_width, image.Width());
            EXPECT_EQ(expected_height, image.Height());
          },
          1));

  CreateNotificationBridgeLinux(TestParams());
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("")
          .SetType(message_center::NOTIFICATION_TYPE_IMAGE)
          .SetImage(original_image)
          .GetResult(),
      nullptr);
}

TEST_F(NotificationPlatformBridgeLinuxTest, NotificationAttribution) {
  EXPECT_CALL(*mock_notification_proxy_.get(),
              CallMethodAndBlock(Calls("Notify"), _))
      .WillOnce(OnNotify(
          [](const NotificationRequest& request) {
            EXPECT_EQ(
                "<a href=\"https://google.com/"
                "search?q=test&amp;ie=UTF8\">google.com</a>\n\nBody text",
                request.body);
            EXPECT_TRUE(request.kde_origin_name.empty());
          },
          1));

  CreateNotificationBridgeLinux(TestParams());
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("")
          .SetMessage(u"Body text")
          .SetOriginUrl(GURL("https://google.com/search?q=test&ie=UTF8"))
          .GetResult(),
      nullptr);
}

TEST_F(NotificationPlatformBridgeLinuxTest, NotificationAttributionKde) {
  EXPECT_CALL(*mock_notification_proxy_.get(),
              CallMethodAndBlock(Calls("Notify"), _))
      .WillOnce(OnNotify(
          [](const NotificationRequest& request) {
            EXPECT_EQ("Body text", request.body);
            EXPECT_EQ("google.com", request.kde_origin_name);
          },
          1));

  CreateNotificationBridgeLinux(TestParams().SetCapabilities(
      std::vector<std::string>{"actions", "body", "x-kde-origin-name"}));
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("")
          .SetMessage(u"Body text")
          .SetOriginUrl(GURL("https://google.com/search?q=test&ie=UTF8"))
          .GetResult(),
      nullptr);
}

TEST_F(NotificationPlatformBridgeLinuxTest, MissingActionsCapability) {
  CreateNotificationBridgeLinux(
      TestParams()
          .SetCapabilities(std::vector<std::string>{"body"})
          .SetExpectInitSuccess(false)
          .SetConnectSignals(false));
}

TEST_F(NotificationPlatformBridgeLinuxTest, MissingBodyCapability) {
  CreateNotificationBridgeLinux(
      TestParams()
          .SetCapabilities(std::vector<std::string>{"actions"})
          .SetExpectInitSuccess(false)
          .SetConnectSignals(false));
}

TEST_F(NotificationPlatformBridgeLinuxTest, EscapeHtml) {
  EXPECT_CALL(*mock_notification_proxy_.get(),
              CallMethodAndBlock(Calls("Notify"), _))
      .WillOnce(OnNotify(
          [](const NotificationRequest& request) {
            EXPECT_EQ("&lt;span id='1' class=\"2\"&gt;&amp;#39;&lt;/span&gt;",
                      request.body);
          },
          1));

  CreateNotificationBridgeLinux(TestParams());
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("")
          .SetMessage(u"<span id='1' class=\"2\">&#39;</span>")
          .GetResult(),
      nullptr);
}

TEST_F(NotificationPlatformBridgeLinuxTest, Silent) {
  EXPECT_CALL(*mock_notification_proxy_.get(),
              CallMethodAndBlock(Calls("Notify"), _))
      .WillOnce(OnNotify(
          [=](const NotificationRequest& request) {
            EXPECT_FALSE(request.silent);
          },
          1))
      .WillOnce(OnNotify(
          [=](const NotificationRequest& request) {
            EXPECT_TRUE(request.silent);
          },
          2));

  CreateNotificationBridgeLinux(TestParams());
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("1").SetSilent(false).GetResult(), nullptr);
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("2").SetSilent(true).GetResult(), nullptr);
}

TEST_F(NotificationPlatformBridgeLinuxTest, OriginUrlFormat) {
  EXPECT_CALL(*mock_notification_proxy_.get(),
              CallMethodAndBlock(Calls("Notify"), _))
      .WillOnce(OnNotify(
          [=](const NotificationRequest& request) {
            EXPECT_EQ("google.com", request.body);
          },
          1))
      .WillOnce(OnNotify(
          [=](const NotificationRequest& request) {
            EXPECT_EQ("mail.google.com", request.body);
          },
          2))
      .WillOnce(OnNotify(
          [=](const NotificationRequest& request) {
            EXPECT_EQ("123.123.123.123", request.body);
          },
          3))
      .WillOnce(OnNotify(
          [=](const NotificationRequest& request) {
            EXPECT_EQ("a.b.c.co.uk", request.body);
          },
          4))
      .WillOnce(OnNotify(
          [=](const NotificationRequest& request) {
            EXPECT_EQ("evilsite.com", request.body);
          },
          4));

  CreateNotificationBridgeLinux(TestParams().SetCapabilities(
      std::vector<std::string>{"actions", "body"}));
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("1")
          .SetOriginUrl(GURL("https://google.com"))
          .GetResult(),
      nullptr);
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("2")
          .SetOriginUrl(GURL("https://mail.google.com"))
          .GetResult(),
      nullptr);
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("3")
          .SetOriginUrl(GURL("https://123.123.123.123"))
          .GetResult(),
      nullptr);
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("4")
          .SetOriginUrl(GURL("https://a.b.c.co.uk/file.html"))
          .GetResult(),
      nullptr);
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("5")
          .SetOriginUrl(GURL(
              "https://google.com.blahblahblahblahblahblahblah.evilsite.com"))
          .GetResult(),
      nullptr);
}

TEST_F(NotificationPlatformBridgeLinuxTest,
       OldCinnamonNotificationsHaveClosebutton) {
  EXPECT_CALL(*mock_notification_proxy_.get(),
              CallMethodAndBlock(Calls("Notify"), _))
      .WillOnce(OnNotify(
          [](const NotificationRequest& request) {
            ASSERT_EQ(3UL, request.actions.size());
            EXPECT_EQ("default", request.actions[0].id);
            EXPECT_EQ("Activate", request.actions[0].label);
            EXPECT_EQ("settings", request.actions[1].id);
            EXPECT_EQ("Settings", request.actions[1].label);
            EXPECT_EQ("close", request.actions[2].id);
            EXPECT_EQ("Close", request.actions[2].label);
          },
          1));

  CreateNotificationBridgeLinux(
      TestParams().SetServerName("cinnamon").SetServerVersion("3.6.7"));
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("").GetResult(), nullptr);
}

TEST_F(NotificationPlatformBridgeLinuxTest,
       NewCinnamonNotificationsDontHaveClosebutton) {
  EXPECT_CALL(*mock_notification_proxy_.get(),
              CallMethodAndBlock(Calls("Notify"), _))
      .WillOnce(OnNotify(
          [](const NotificationRequest& request) {
            ASSERT_EQ(2UL, request.actions.size());
            EXPECT_EQ("default", request.actions[0].id);
            EXPECT_EQ("Activate", request.actions[0].label);
            EXPECT_EQ("settings", request.actions[1].id);
            EXPECT_EQ("Settings", request.actions[1].label);
          },
          1));

  CreateNotificationBridgeLinux(
      TestParams().SetServerName("cinnamon").SetServerVersion("3.8.0"));
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("").GetResult(), nullptr);
}

TEST_F(NotificationPlatformBridgeLinuxTest, NoSettingsButton) {
  EXPECT_CALL(*mock_notification_proxy_.get(),
              CallMethodAndBlock(Calls("Notify"), _))
      .WillOnce(OnNotify(
          [](const NotificationRequest& request) {
            ASSERT_EQ(1UL, request.actions.size());
            EXPECT_EQ("default", request.actions[0].id);
            EXPECT_EQ("Activate", request.actions[0].label);
          },
          1));

  CreateNotificationBridgeLinux(
      TestParams().SetServerName("cinnamon").SetServerVersion("3.8.0"));
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("")
          .SetSettingsButtonHandler(SettingsButtonHandler::NONE)
          .GetResult(),
      nullptr);
}

TEST_F(NotificationPlatformBridgeLinuxTest, DefaultButtonForwards) {
  EXPECT_CALL(*mock_notification_proxy_.get(),
              CallMethodAndBlock(Calls("Notify"), _))
      .WillOnce(OnNotify([](const NotificationRequest&) {}, 1));

  CreateNotificationBridgeLinux(TestParams());
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("1")
          .SetOriginUrl(GURL("https://google.com"))
          .GetResult(),
      nullptr);

  InvokeAction(1, "default");

  content::RunAllTasksUntilIdle();

  EXPECT_EQ(NotificationOperation::kClick, last_operation_);
  EXPECT_EQ(false, last_action_index_.has_value());
}

TEST_F(NotificationPlatformBridgeLinuxTest, SettingsButtonForwards) {
  EXPECT_CALL(*mock_notification_proxy_.get(),
              CallMethodAndBlock(Calls("Notify"), _))
      .WillOnce(OnNotify([](const NotificationRequest&) {}, 1));

  CreateNotificationBridgeLinux(TestParams());
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("1")
          .SetOriginUrl(GURL("https://google.com"))
          .GetResult(),
      nullptr);

  InvokeAction(1, "settings");

  content::RunAllTasksUntilIdle();

  EXPECT_EQ(NotificationOperation::kSettings, last_operation_);
}

TEST_F(NotificationPlatformBridgeLinuxTest, ActionButtonForwards) {
  EXPECT_CALL(*mock_notification_proxy_.get(),
              CallMethodAndBlock(Calls("Notify"), _))
      .WillOnce(OnNotify([](const NotificationRequest&) {}, 1));

  CreateNotificationBridgeLinux(TestParams());
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("1")
          .SetOriginUrl(GURL("https://google.com"))
          .AddButton(ButtonInfo(u"button0"))
          .AddButton(ButtonInfo(u"button1"))
          .GetResult(),
      nullptr);

  InvokeAction(1, "1");

  content::RunAllTasksUntilIdle();

  EXPECT_EQ(NotificationOperation::kClick, last_operation_);
  EXPECT_EQ(1, last_action_index_);
}

TEST_F(NotificationPlatformBridgeLinuxTest, CloseButtonForwards) {
  EXPECT_CALL(*mock_notification_proxy_.get(),
              CallMethodAndBlock(Calls("Notify"), _))
      .WillOnce(OnNotify([](const NotificationRequest&) {}, 1));
  EXPECT_CALL(*mock_notification_proxy_.get(),
              CallMethodAndBlock(Calls("CloseNotification"), _))
      .WillOnce(OnCloseNotification());

  // custom close button is only added on cinnamon
  CreateNotificationBridgeLinux(TestParams().SetServerName("cinnamon"));
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("1")
          .SetOriginUrl(GURL("https://google.com"))
          .GetResult(),
      nullptr);

  InvokeAction(1, "close");

  content::RunAllTasksUntilIdle();

  EXPECT_EQ(NotificationOperation::kClose, last_operation_);
}

TEST_F(NotificationPlatformBridgeLinuxTest, NotificationRepliedForwards) {
  EXPECT_CALL(*mock_notification_proxy_.get(),
              CallMethodAndBlock(Calls("Notify"), _))
      .WillOnce(OnNotify([](const NotificationRequest&) {}, 1));

  CreateNotificationBridgeLinux(TestParams().SetCapabilities(
      std::vector<std::string>{"actions", "body", "inline-reply"}));

  ButtonInfo replyButton(u"button0");
  replyButton.placeholder = u"Reply...";

  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("1").AddButton(replyButton).GetResult(), nullptr);

  ReplyToNotification(1, "Hello");

  content::RunAllTasksUntilIdle();

  EXPECT_EQ(NotificationOperation::kClick, last_operation_);
  EXPECT_EQ(false, last_action_index_.has_value());
  EXPECT_EQ(u"Hello", last_reply_);
}
