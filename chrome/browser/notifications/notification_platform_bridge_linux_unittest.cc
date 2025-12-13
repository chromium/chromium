// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_linux.h"

#include <dbus/dbus-shared.h>

#include <map>
#include <memory>
#include <vector>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/number_formatting.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/nix/xdg_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/common/notifications/notification_operation.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "content/public/test/test_utils.h"
#include "dbus/mock_bus.h"
#include "dbus/object_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

using message_center::ButtonInfo;
using message_center::Notification;
using message_center::SettingsButtonHandler;

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
      : capabilities{"actions", "body", "body-hyperlinks", "body-images",
                     "body-markup"},
        server_name("NPBL_unittest"),
        server_version("1.0") {}

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

  TestParams& SetConnectSignals(bool new_connect_signals) {
    this->connect_signals = new_connect_signals;
    return *this;
  }

  bool name_has_owner = true;
  std::vector<std::string> capabilities;
  std::string server_name;
  std::string server_version;
  bool expect_init_success = true;
  bool connect_signals = true;
};

NotificationRequest ParseRequest(dbus::MethodCall* method_call) {
  // The "Notify" message must have type (susssasa{sv}i).
  // https://developer.gnome.org/notification-spec/#command-notify
  NotificationRequest request;

  dbus::MessageReader reader(method_call);
  std::string str;
  uint32_t replaces_id;
  EXPECT_TRUE(reader.PopString(&str));              // app_name
  EXPECT_TRUE(reader.PopUint32(&replaces_id));      // replaces_id
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

class FakeDBusProxy : public dbus::ObjectProxy {
 public:
  FakeDBusProxy(dbus::Bus* bus, const dbus::ObjectPath& object_path)
      : dbus::ObjectProxy(bus, DBUS_SERVICE_DBUS, object_path, 0) {}

  void CallMethod(dbus::MethodCall* method_call,
                  int timeout_ms,
                  ResponseCallback callback) override {
    if (method_call->GetMember() == "NameHasOwner") {
      auto response = dbus::Response::CreateEmpty();
      dbus::MessageWriter writer(response.get());
      writer.AppendBool(name_has_owner_);
      std::move(callback).Run(response.get());
      return;
    }

    FAIL() << "Unexpected method call: " << method_call->ToString();
  }

  void SetNameHasOwner(bool name_has_owner) {
    name_has_owner_ = name_has_owner;
  }

 protected:
  ~FakeDBusProxy() override = default;

 private:
  bool name_has_owner_ = true;
};

class FakeNotificationProxy : public dbus::ObjectProxy {
 public:
  using NotifyCallback =
      base::RepeatingCallback<uint32_t(const NotificationRequest&)>;

  FakeNotificationProxy(dbus::Bus* bus, const dbus::ObjectPath& object_path)
      : dbus::ObjectProxy(bus, kFreedesktopNotificationsName, object_path, 0) {}

  void CallMethodWithErrorResponse(dbus::MethodCall* method_call,
                                   int timeout_ms,
                                   ResponseOrErrorCallback callback) override {
    if (method_call->GetMember() == "GetCapabilities") {
      auto response = dbus::Response::CreateEmpty();
      dbus::MessageWriter writer(response.get());
      writer.AppendArrayOfStrings(capabilities_);
      std::move(callback).Run(response.get(), nullptr);
      return;
    }

    if (method_call->GetMember() == "GetServerInformation") {
      auto response = dbus::Response::CreateEmpty();
      dbus::MessageWriter writer(response.get());
      writer.AppendString(server_name_);     // name
      writer.AppendString("chromium");       // vendor
      writer.AppendString(server_version_);  // version
      writer.AppendString("1.2");            // spec_version
      std::move(callback).Run(response.get(), nullptr);
      return;
    }

    if (method_call->GetMember() == "Notify") {
      OnNotify(method_call, std::move(callback));
      return;
    }

    if (method_call->GetMember() == "CloseNotification") {
      OnCloseNotification(method_call, std::move(callback));
      return;
    }

    FAIL() << "Unexpected method call: " << method_call->ToString();
  }

  void ConnectToSignal(const std::string& interface_name,
                       const std::string& signal_name,
                       SignalCallback signal_callback,
                       OnConnectedCallback on_connected_callback) override {
    if (connect_signals_) {
      signal_callbacks_[signal_name] = signal_callback;
    }
    std::move(on_connected_callback)
        .Run(interface_name, signal_name, connect_signals_);
  }

  void FireSignal(const std::string& signal_name, dbus::Signal* signal) {
    auto it = signal_callbacks_.find(signal_name);
    ASSERT_NE(it, signal_callbacks_.end())
        << "Signal not connected: " << signal_name;
    it->second.Run(signal);
  }

  void SetCapabilities(const std::vector<std::string>& capabilities) {
    capabilities_ = capabilities;
  }

  void SetServerName(const std::string& server_name) {
    server_name_ = server_name;
  }

  void SetServerVersion(const std::string& server_version) {
    server_version_ = server_version;
  }

  void SetConnectSignals(bool connect_signals) {
    connect_signals_ = connect_signals;
  }

  void SetNotificationId(uint32_t id) { id_ = id; }

  void SetNotifyCallback(NotifyCallback callback) {
    notify_callback_ = callback;
  }

  int notify_calls() const { return notify_calls_; }
  int close_calls() const { return close_calls_; }
  uint32_t last_closed_id() const { return last_closed_id_; }

 protected:
  ~FakeNotificationProxy() override = default;

 private:
  void OnNotify(dbus::MethodCall* method_call,
                ResponseOrErrorCallback callback) {
    ++notify_calls_;
    NotificationRequest request = ParseRequest(method_call);
    uint32_t id = id_;
    if (notify_callback_) {
      id = notify_callback_.Run(request);
    }

    auto response = GetIdResponse(id);
    std::move(callback).Run(response.get(), nullptr);
  }

  void OnCloseNotification(dbus::MethodCall* method_call,
                           ResponseOrErrorCallback callback) {
    // The "CloseNotification" message must have type (u).
    // https://developer.gnome.org/notification-spec/#command-close-notification

    dbus::MessageReader reader(method_call);
    uint32_t id;
    EXPECT_TRUE(reader.PopUint32(&id));
    EXPECT_FALSE(reader.HasMoreData());

    last_closed_id_ = id;
    ++close_calls_;

    auto response = dbus::Response::CreateEmpty();
    std::move(callback).Run(response.get(), nullptr);
  }

  std::vector<std::string> capabilities_;
  std::string server_name_;
  std::string server_version_;
  bool connect_signals_ = true;
  uint32_t id_ = 1;
  NotifyCallback notify_callback_;
  std::map<std::string, SignalCallback> signal_callbacks_;

  int notify_calls_ = 0;
  int close_calls_ = 0;
  uint32_t last_closed_id_ = 0;
};

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
    fake_dbus_proxy_ = base::MakeRefCounted<FakeDBusProxy>(
        mock_bus_.get(), dbus::ObjectPath(DBUS_PATH_DBUS));
    fake_notification_proxy_ = base::MakeRefCounted<FakeNotificationProxy>(
        mock_bus_.get(), dbus::ObjectPath(kFreedesktopNotificationsPath));
  }

  void TearDown() override {
    notification_bridge_linux_->CleanUp();
    content::RunAllTasksUntilIdle();
    notification_bridge_linux_.reset();
    display_service_tester_.reset();
    fake_dbus_proxy_.reset();
    fake_notification_proxy_ = nullptr;
    mock_bus_ = nullptr;
    BrowserWithTestWindowTest::TearDown();
  }

  void HandleOperation(NotificationOperation operation,
                       NotificationHandler::Type notification_type,
                       const GURL& origin,
                       const std::string& notification_id,
                       const std::optional<int>& action_index,
                       const std::optional<std::u16string>& reply,
                       const std::optional<bool>& by_user,
                       const std::optional<bool>& is_suspicious) {
    last_operation_ = operation;
    last_action_index_ = action_index;
    last_reply_ = reply;
  }

 protected:
  void CreateNotificationBridgeLinux(const TestParams& test_params) {
    test_params_ = test_params;

    EXPECT_CALL(
        *mock_bus_.get(),
        GetObjectProxy(DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS)))
        .WillOnce(testing::Return(fake_dbus_proxy_.get()));
    EXPECT_CALL(*mock_bus_.get(),
                GetObjectProxy(kFreedesktopNotificationsName,
                               dbus::ObjectPath(kFreedesktopNotificationsPath)))
        .WillOnce(testing::Return(fake_notification_proxy_.get()));

    fake_dbus_proxy_->SetNameHasOwner(test_params.name_has_owner);

    fake_notification_proxy_->SetCapabilities(test_params.capabilities);
    fake_notification_proxy_->SetConnectSignals(test_params.connect_signals);

    if (test_params.expect_init_success) {
      fake_notification_proxy_->SetServerName(test_params.server_name);
      fake_notification_proxy_->SetServerVersion(test_params.server_version);
    }

    notification_bridge_linux_ =
        base::WrapUnique(new NotificationPlatformBridgeLinux(mock_bus_));
    notification_bridge_linux_->SetReadyCallback(
        base::BindOnce(&NotificationPlatformBridgeLinuxTest::
                           MockableNotificationBridgeReadyCallback,
                       base::Unretained(this)));
    content::RunAllTasksUntilIdle();
    EXPECT_TRUE(ready_called_);
  }

  void SendActivationToken(uint32_t dbus_id, const std::string& token) {
    dbus::Signal signal(kFreedesktopNotificationsName, "ActivationToken");
    dbus::MessageWriter writer(&signal);
    writer.AppendUint32(dbus_id);
    writer.AppendString(token);
    fake_notification_proxy_->FireSignal("ActivationToken", &signal);
  }

  void InvokeAction(uint32_t dbus_id, const std::string& action) {
    dbus::Signal signal(kFreedesktopNotificationsName, "ActionInvoked");
    dbus::MessageWriter writer(&signal);
    writer.AppendUint32(dbus_id);
    writer.AppendString(action);
    fake_notification_proxy_->FireSignal("ActionInvoked", &signal);
  }

  void ReplyToNotification(uint32_t dbus_id, const std::string& message) {
    dbus::Signal signal(kFreedesktopNotificationsName, "NotificationReplied");
    dbus::MessageWriter writer(&signal);
    writer.AppendUint32(dbus_id);
    writer.AppendString(message);
    fake_notification_proxy_->FireSignal("NotificationReplied", &signal);
  }

  void NotificationClosed(uint32_t dbus_id, uint32_t reason) {
    dbus::Signal signal(kFreedesktopNotificationsName, "NotificationClosed");
    dbus::MessageWriter writer(&signal);
    writer.AppendUint32(dbus_id);
    writer.AppendUint32(reason);
    fake_notification_proxy_->FireSignal("NotificationClosed", &signal);
  }

  void MockableNotificationBridgeReadyCallback(bool success) {
    EXPECT_FALSE(ready_called_);
    ready_called_ = true;
    EXPECT_EQ(test_params_.expect_init_success, success);
  }

  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<FakeDBusProxy> fake_dbus_proxy_;
  scoped_refptr<FakeNotificationProxy> fake_notification_proxy_;

  std::unique_ptr<NotificationPlatformBridgeLinux> notification_bridge_linux_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_tester_;

  std::optional<NotificationOperation> last_operation_;
  std::optional<int> last_action_index_;
  std::optional<std::u16string> last_reply_;
  TestParams test_params_;
  bool ready_called_ = false;
};

TEST_F(NotificationPlatformBridgeLinuxTest, SetUpAndTearDown) {
  CreateNotificationBridgeLinux(TestParams());
}

TEST_F(NotificationPlatformBridgeLinuxTest, NotifyAndCloseFormat) {
  fake_notification_proxy_->SetNotificationId(1);

  CreateNotificationBridgeLinux(TestParams());
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("").GetResult(), nullptr);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1, fake_notification_proxy_->notify_calls());

  notification_bridge_linux_->Close(profile(), "");
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1, fake_notification_proxy_->close_calls());
  EXPECT_EQ(1u, fake_notification_proxy_->last_closed_id());
}

TEST_F(NotificationPlatformBridgeLinuxTest, ProgressPercentageAddedToSummary) {
  fake_notification_proxy_->SetNotifyCallback(base::BindLambdaForTesting(
      [](const NotificationRequest& request) -> uint32_t {
        EXPECT_EQ(base::UTF16ToUTF8(base::FormatPercent(42)) + " - The Title",
                  request.summary);
        return 1;
      }));

  CreateNotificationBridgeLinux(TestParams());
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("")
          .SetType(message_center::NOTIFICATION_TYPE_PROGRESS)
          .SetProgress(42)
          .SetTitle(u"The Title")
          .GetResult(),
      nullptr);
  content::RunAllTasksUntilIdle();
}

TEST_F(NotificationPlatformBridgeLinuxTest, NotificationListItemsInBody) {
  fake_notification_proxy_->SetNotifyCallback(base::BindLambdaForTesting(
      [](const NotificationRequest& request) -> uint32_t {
        EXPECT_EQ("<b>abc</b> 123\n<b>def</b> 456", request.body);
        return 1;
      }));

  CreateNotificationBridgeLinux(TestParams());
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("")
          .SetType(message_center::NOTIFICATION_TYPE_MULTIPLE)
          .SetItems(std::vector<message_center::NotificationItem>{
              {u"abc", u"123"}, {u"def", u"456"}})
          .GetResult(),
      nullptr);
  content::RunAllTasksUntilIdle();
}

TEST_F(NotificationPlatformBridgeLinuxTest, NotificationTimeoutsNoPersistence) {
  const int32_t kExpireTimeout = 25000;
  const int32_t kExpireTimeoutNever = 0;

  int call_count = 0;
  fake_notification_proxy_->SetNotifyCallback(base::BindLambdaForTesting(
      [&](const NotificationRequest& request) -> uint32_t {
        call_count++;
        if (call_count == 1) {
          EXPECT_EQ(kExpireTimeout, request.expire_timeout);
          return 1;
        } else {
          EXPECT_EQ(kExpireTimeoutNever, request.expire_timeout);
          return 2;
        }
      }));

  CreateNotificationBridgeLinux(TestParams());
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("1").SetNeverTimeout(false).GetResult(), nullptr);
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("2").SetNeverTimeout(true).GetResult(), nullptr);
  content::RunAllTasksUntilIdle();
}

TEST_F(NotificationPlatformBridgeLinuxTest,
       NotificationTimeoutWithPersistence) {
  const int32_t kExpireTimeoutDefault = -1;
  fake_notification_proxy_->SetNotifyCallback(base::BindLambdaForTesting(
      [=](const NotificationRequest& request) -> uint32_t {
        EXPECT_EQ(kExpireTimeoutDefault, request.expire_timeout);
        return 1;
      }));

  CreateNotificationBridgeLinux(TestParams().SetCapabilities(
      std::vector<std::string>{"actions", "body", "persistence"}));
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("1").GetResult(), nullptr);
  content::RunAllTasksUntilIdle();
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

  fake_notification_proxy_->SetNotifyCallback(base::BindLambdaForTesting(
      [=](const NotificationRequest& request) -> uint32_t {
        std::string file_name;
        EXPECT_TRUE(RE2::FullMatch(
            request.body, "\\<img src=\\\"file://(.+)\\\" alt=\\\".*\\\"/\\>",
            &file_name));
        std::optional<std::vector<uint8_t>> file_contents =
            base::ReadFileToBytes(base::FilePath(file_name));
        if (!file_contents) {
          ADD_FAILURE() << "Failed to read file: " << file_name;
          return 1;
        }
        gfx::Image image = gfx::Image::CreateFrom1xPNGBytes(*file_contents);
        EXPECT_EQ(expected_width, image.Width());
        EXPECT_EQ(expected_height, image.Height());
        return 1;
      }));

  CreateNotificationBridgeLinux(TestParams());
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("")
          .SetType(message_center::NOTIFICATION_TYPE_IMAGE)
          .SetImage(original_image)
          .GetResult(),
      nullptr);
  content::RunAllTasksUntilIdle();
}

TEST_F(NotificationPlatformBridgeLinuxTest, NotificationAttribution) {
  fake_notification_proxy_->SetNotifyCallback(base::BindLambdaForTesting(
      [](const NotificationRequest& request) -> uint32_t {
        EXPECT_EQ(
            "<a href=\"https://google.com/"
            "search?q=test&amp;ie=UTF8\">google.com</a>\n\nBody text",
            request.body);
        EXPECT_TRUE(request.kde_origin_name.empty());
        return 1;
      }));

  CreateNotificationBridgeLinux(TestParams());
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("")
          .SetMessage(u"Body text")
          .SetOriginUrl(GURL("https://google.com/search?q=test&ie=UTF8"))
          .GetResult(),
      nullptr);
  content::RunAllTasksUntilIdle();
}

TEST_F(NotificationPlatformBridgeLinuxTest, NotificationAttributionKde) {
  fake_notification_proxy_->SetNotifyCallback(base::BindLambdaForTesting(
      [](const NotificationRequest& request) -> uint32_t {
        EXPECT_EQ("Body text", request.body);
        EXPECT_EQ("google.com", request.kde_origin_name);
        return 1;
      }));

  CreateNotificationBridgeLinux(TestParams().SetCapabilities(
      std::vector<std::string>{"actions", "body", "x-kde-origin-name"}));
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("")
          .SetMessage(u"Body text")
          .SetOriginUrl(GURL("https://google.com/search?q=test&ie=UTF8"))
          .GetResult(),
      nullptr);
  content::RunAllTasksUntilIdle();
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
  fake_notification_proxy_->SetNotifyCallback(base::BindLambdaForTesting(
      [](const NotificationRequest& request) -> uint32_t {
        EXPECT_EQ("&lt;span id='1' class=\"2\"&gt;&amp;#39;&lt;/span&gt;",
                  request.body);
        return 1;
      }));

  CreateNotificationBridgeLinux(TestParams());
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("")
          .SetMessage(u"<span id='1' class=\"2\">&#39;</span>")
          .GetResult(),
      nullptr);
  content::RunAllTasksUntilIdle();
}

TEST_F(NotificationPlatformBridgeLinuxTest, Silent) {
  int call_count = 0;
  fake_notification_proxy_->SetNotifyCallback(base::BindLambdaForTesting(
      [&](const NotificationRequest& request) -> uint32_t {
        call_count++;
        if (call_count == 1) {
          EXPECT_FALSE(request.silent);
          return 1;
        } else {
          EXPECT_TRUE(request.silent);
          return 2;
        }
      }));

  CreateNotificationBridgeLinux(TestParams());
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("1").SetSilent(false).GetResult(), nullptr);
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("2").SetSilent(true).GetResult(), nullptr);
  content::RunAllTasksUntilIdle();
}

TEST_F(NotificationPlatformBridgeLinuxTest, OriginUrlFormat) {
  int call_count = 0;
  fake_notification_proxy_->SetNotifyCallback(base::BindLambdaForTesting(
      [&](const NotificationRequest& request) -> uint32_t {
        call_count++;
        switch (call_count) {
          case 1:
            EXPECT_EQ("google.com", request.body);
            break;
          case 2:
            EXPECT_EQ("mail.google.com", request.body);
            break;
          case 3:
            EXPECT_EQ("123.123.123.123", request.body);
            break;
          case 4:
            EXPECT_EQ("a.b.c.co.uk", request.body);
            break;
          case 5:
            EXPECT_EQ("evilsite.com", request.body);
            break;
        }
        return call_count;
      }));

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
  content::RunAllTasksUntilIdle();
}

TEST_F(NotificationPlatformBridgeLinuxTest,
       OldCinnamonNotificationsHaveClosebutton) {
  fake_notification_proxy_->SetNotifyCallback(base::BindLambdaForTesting(
      [](const NotificationRequest& request) -> uint32_t {
        if (request.actions.size() == 3) {
          EXPECT_EQ("default", request.actions[0].id);
          EXPECT_EQ("Activate", request.actions[0].label);
          EXPECT_EQ("settings", request.actions[1].id);
          EXPECT_EQ("Settings", request.actions[1].label);
          EXPECT_EQ("close", request.actions[2].id);
          EXPECT_EQ("Close", request.actions[2].label);
        } else {
          ADD_FAILURE() << "Expected 3 actions, got " << request.actions.size();
        }
        return 1;
      }));

  CreateNotificationBridgeLinux(
      TestParams().SetServerName("cinnamon").SetServerVersion("3.6.7"));
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("").GetResult(), nullptr);
  content::RunAllTasksUntilIdle();
}

TEST_F(NotificationPlatformBridgeLinuxTest,
       NewCinnamonNotificationsDontHaveClosebutton) {
  fake_notification_proxy_->SetNotifyCallback(base::BindLambdaForTesting(
      [](const NotificationRequest& request) -> uint32_t {
        if (request.actions.size() == 2) {
          EXPECT_EQ("default", request.actions[0].id);
          EXPECT_EQ("Activate", request.actions[0].label);
          EXPECT_EQ("settings", request.actions[1].id);
          EXPECT_EQ("Settings", request.actions[1].label);
        } else {
          ADD_FAILURE() << "Expected 2 actions, got " << request.actions.size();
        }
        return 1;
      }));

  CreateNotificationBridgeLinux(
      TestParams().SetServerName("cinnamon").SetServerVersion("3.8.0"));
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("").GetResult(), nullptr);
  content::RunAllTasksUntilIdle();
}

TEST_F(NotificationPlatformBridgeLinuxTest, NoSettingsButton) {
  fake_notification_proxy_->SetNotifyCallback(base::BindLambdaForTesting(
      [](const NotificationRequest& request) -> uint32_t {
        if (request.actions.size() == 1) {
          EXPECT_EQ("default", request.actions[0].id);
          EXPECT_EQ("Activate", request.actions[0].label);
        } else {
          ADD_FAILURE() << "Expected 1 action, got " << request.actions.size();
        }
        return 1;
      }));

  CreateNotificationBridgeLinux(
      TestParams().SetServerName("cinnamon").SetServerVersion("3.8.0"));
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("")
          .SetSettingsButtonHandler(SettingsButtonHandler::NONE)
          .GetResult(),
      nullptr);
  content::RunAllTasksUntilIdle();
}

TEST_F(NotificationPlatformBridgeLinuxTest, ActivationToken) {
  static constexpr char kToken[] = "test-token";
  CreateNotificationBridgeLinux(TestParams());
  SendActivationToken(1, kToken);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(base::nix::TakeXdgActivationToken(), kToken);
}

TEST_F(NotificationPlatformBridgeLinuxTest, DefaultButtonForwards) {
  fake_notification_proxy_->SetNotificationId(1);

  CreateNotificationBridgeLinux(TestParams());
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("1")
          .SetOriginUrl(GURL("https://google.com"))
          .GetResult(),
      nullptr);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1, fake_notification_proxy_->notify_calls());

  InvokeAction(1, "default");

  EXPECT_EQ(NotificationOperation::kClick, last_operation_);
  EXPECT_EQ(false, last_action_index_.has_value());
}

TEST_F(NotificationPlatformBridgeLinuxTest, SettingsButtonForwards) {
  fake_notification_proxy_->SetNotificationId(1);

  CreateNotificationBridgeLinux(TestParams());
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("1")
          .SetOriginUrl(GURL("https://google.com"))
          .GetResult(),
      nullptr);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1, fake_notification_proxy_->notify_calls());

  InvokeAction(1, "settings");

  EXPECT_EQ(NotificationOperation::kSettings, last_operation_);
}

TEST_F(NotificationPlatformBridgeLinuxTest, ActionButtonForwards) {
  fake_notification_proxy_->SetNotificationId(1);

  CreateNotificationBridgeLinux(TestParams());
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("1")
          .SetOriginUrl(GURL("https://google.com"))
          .AddButton(ButtonInfo(u"button0"))
          .AddButton(ButtonInfo(u"button1"))
          .GetResult(),
      nullptr);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1, fake_notification_proxy_->notify_calls());

  InvokeAction(1, "1");

  EXPECT_EQ(NotificationOperation::kClick, last_operation_);
  EXPECT_EQ(1, last_action_index_);
}

TEST_F(NotificationPlatformBridgeLinuxTest, CloseButtonForwards) {
  fake_notification_proxy_->SetNotificationId(1);

  // The custom close button is only auto-added on older Cinnamon.
  CreateNotificationBridgeLinux(TestParams().SetServerName("cinnamon"));
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("1")
          .SetOriginUrl(GURL("https://google.com"))
          .GetResult(),
      nullptr);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1, fake_notification_proxy_->notify_calls());

  InvokeAction(1, "close");

  EXPECT_EQ(NotificationOperation::kClose, last_operation_);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1, fake_notification_proxy_->close_calls());
  EXPECT_EQ(1u, fake_notification_proxy_->last_closed_id());
}

TEST_F(NotificationPlatformBridgeLinuxTest, NotificationRepliedForwards) {
  fake_notification_proxy_->SetNotificationId(1);

  CreateNotificationBridgeLinux(TestParams().SetCapabilities(
      std::vector<std::string>{"actions", "body", "inline-reply"}));

  ButtonInfo replyButton(u"button0");
  replyButton.placeholder = u"Reply...";

  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("1").AddButton(replyButton).GetResult(), nullptr);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1, fake_notification_proxy_->notify_calls());

  ReplyToNotification(1, "Hello");

  EXPECT_EQ(NotificationOperation::kClick, last_operation_);
  EXPECT_FALSE(last_action_index_.has_value());
  EXPECT_EQ(u"Hello", last_reply_);
}

TEST_F(NotificationPlatformBridgeLinuxTest, NotificationClosedForwards) {
  fake_notification_proxy_->SetNotificationId(1);

  CreateNotificationBridgeLinux(TestParams());
  notification_bridge_linux_->Display(
      NotificationHandler::Type::WEB_PERSISTENT, profile(),
      NotificationBuilder("1").GetResult(), nullptr);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1, fake_notification_proxy_->notify_calls());

  NotificationClosed(1, 3 /* closed by user */);

  EXPECT_EQ(NotificationOperation::kClose, last_operation_);
}
