// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/global_shortcut_listener_linux.h"

#include "base/memory/scoped_refptr.h"
#include "base/nix/xdg_util.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "components/dbus/properties/types.h"
#include "content/public/test/browser_task_environment.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::Return;

namespace extensions {

namespace {

constexpr uint32_t kResponseSuccess = 0;
constexpr char kBusName[] = ":1.456";
constexpr char kExtensionId[] = "test_extension_id";
constexpr char kProfileId[] = "test_profile_id";
constexpr char kCommandName[] = "test_command";
constexpr char16_t kShortcutDescription[] = u"Test Shortcut Description";

MATCHER_P2(MatchMethod, interface, member, "") {
  return arg->GetInterface() == interface && arg->GetMember() == member;
}

}  // namespace

using DbusShortcuts = DbusArray<DbusStruct<DbusString, DbusDictionary>>;

class MockObserver final : public GlobalShortcutListener::Observer {
 public:
  virtual ~MockObserver() = default;

  void OnKeyPressed(const ui::Accelerator& accelerator) override {
    // GlobalShortcutListenerLinux uses ExecuteCommand() instead.
    NOTREACHED();
  }

  MOCK_METHOD2(ExecuteCommand,
               void(const std::string& extension_id,
                    const std::string& command_name));
};

TEST(GlobalShortcutListenerLinuxTest, OnCommandsChanged) {
  // A UI environment is required since GlobalShortcutListener (base class of
  // GlobalShortcutListenerLinux) CHECKs that it's running on a UI thread.
  content::BrowserTaskEnvironment task_environment;

  auto mock_bus = base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());

  auto mock_dbus_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      mock_bus.get(), DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));

  auto mock_global_shortcuts_proxy =
      base::MakeRefCounted<dbus::MockObjectProxy>(
          mock_bus.get(), GlobalShortcutListenerLinux::kPortalServiceName,
          dbus::ObjectPath(GlobalShortcutListenerLinux::kPortalObjectPath));

  auto mock_systemd_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      mock_bus.get(), "org.freedesktop.systemd1",
      dbus::ObjectPath("/org/freedesktop/systemd1"));
  EXPECT_CALL(*mock_bus,
              GetObjectProxy("org.freedesktop.systemd1",
                             dbus::ObjectPath("/org/freedesktop/systemd1")))
      .Times(AtLeast(0))
      .WillRepeatedly(Return(mock_systemd_proxy.get()));
  EXPECT_CALL(*mock_systemd_proxy, DoCallMethod(_, _, _))
      .Times(AtLeast(0))
      .WillRepeatedly(Invoke([](dbus::MethodCall*, int,
                                dbus::ObjectProxy::ResponseCallback* callback) {
        std::move(*callback).Run(nullptr);
      }));

  EXPECT_CALL(*mock_bus, AssertOnOriginThread()).WillRepeatedly([] {});

  EXPECT_CALL(*mock_bus, GetObjectProxy(DBUS_SERVICE_DBUS,
                                        dbus::ObjectPath(DBUS_PATH_DBUS)))
      .WillRepeatedly(Return(mock_dbus_proxy.get()));

  EXPECT_CALL(
      *mock_bus,
      GetObjectProxy(
          GlobalShortcutListenerLinux::kPortalServiceName,
          dbus::ObjectPath(GlobalShortcutListenerLinux::kPortalObjectPath)))
      .WillRepeatedly(Return(mock_global_shortcuts_proxy.get()));

  EXPECT_CALL(*mock_bus, GetConnectionName()).WillRepeatedly(Return(kBusName));

  // CheckForServiceAndStart
  EXPECT_CALL(
      *mock_dbus_proxy,
      DoCallMethod(MatchMethod(DBUS_INTERFACE_DBUS, "NameHasOwner"), _, _))
      .WillOnce(Invoke([](dbus::MethodCall* method_call, int timeout_ms,
                          dbus::ObjectProxy::ResponseCallback* callback) {
        dbus::MessageReader reader(method_call);
        std::string service_name;
        EXPECT_TRUE(reader.PopString(&service_name));
        EXPECT_EQ(service_name, "org.freedesktop.systemd1");

        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendBool(true);
        std::move(*callback).Run(response.get());
      }))
      .WillOnce(Invoke([](dbus::MethodCall* method_call, int timeout_ms,
                          dbus::ObjectProxy::ResponseCallback* callback) {
        dbus::MessageReader reader(method_call);
        std::string service_name;
        EXPECT_TRUE(reader.PopString(&service_name));
        EXPECT_EQ(service_name,
                  GlobalShortcutListenerLinux::kPortalServiceName);

        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendBool(true);
        std::move(*callback).Run(response.get());
      }));

  // Activated signal
  dbus::ObjectProxy::SignalCallback activated_callback;
  EXPECT_CALL(
      *mock_global_shortcuts_proxy,
      DoConnectToSignal(GlobalShortcutListenerLinux::kGlobalShortcutsInterface,
                        GlobalShortcutListenerLinux::kSignalActivated, _, _))
      .WillOnce(Invoke(
          [&](const std::string& interface_name, const std::string& signal_name,
              dbus::ObjectProxy::SignalCallback signal_callback,
              dbus::ObjectProxy::OnConnectedCallback* on_connected_callback) {
            // Simulate successful connection
            std::move(*on_connected_callback)
                .Run(interface_name, signal_name, true);

            // Save the signal callback for later use
            activated_callback = signal_callback;
          }));

  auto global_shortcut_listener =
      std::make_unique<GlobalShortcutListenerLinux>(mock_bus);
  auto observer = std::make_unique<MockObserver>();

  // These object proxies have unique generated names, so are initialized when
  // GetObjectProxy() is called.
  scoped_refptr<dbus::MockObjectProxy> create_session_request_proxy;
  scoped_refptr<dbus::MockObjectProxy> list_shortcuts_request_proxy;
  scoped_refptr<dbus::MockObjectProxy> bind_shortcuts_request_proxy;
  scoped_refptr<dbus::MockObjectProxy> session_proxy;

  auto get_object_proxy_session =
      [&](std::string_view service_name,
          const dbus::ObjectPath& object_path) -> dbus::ObjectProxy* {
    // The first call in the sequence is for the session proxy.
    session_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
        mock_bus.get(), GlobalShortcutListenerLinux::kPortalServiceName,
        object_path);
    return session_proxy.get();
  };

  auto get_object_proxy_create_session =
      [&](std::string_view service_name,
          const dbus::ObjectPath& object_path) -> dbus::ObjectProxy* {
    // CreateSession
    create_session_request_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
        mock_bus.get(), GlobalShortcutListenerLinux::kPortalServiceName,
        object_path);
    EXPECT_CALL(*create_session_request_proxy, DoConnectToSignal(_, _, _, _))
        .WillOnce(Invoke(
            [&](const std::string& interface_name,
                const std::string& signal_name,
                dbus::ObjectProxy::SignalCallback signal_callback,
                dbus::ObjectProxy::OnConnectedCallback* on_connected_callback) {
              EXPECT_EQ(interface_name, "org.freedesktop.portal.Request");
              EXPECT_EQ(signal_name, "Response");

              std::move(*on_connected_callback)
                  .Run(interface_name, signal_name, true);

              dbus::Signal signal(interface_name, signal_name);
              dbus::MessageWriter writer(&signal);
              writer.AppendUint32(kResponseSuccess);
              MakeDbusDictionary(
                  "session_handle",
                  DbusString(session_proxy->object_path().value()))
                  .Write(&writer);
              signal_callback.Run(&signal);
            }));
    return create_session_request_proxy.get();
  };

  auto get_object_proxy_list_shortcuts =
      [&](std::string_view service_name,
          const dbus::ObjectPath& object_path) -> dbus::ObjectProxy* {
    // ListShortcuts
    list_shortcuts_request_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
        mock_bus.get(), GlobalShortcutListenerLinux::kPortalServiceName,
        object_path);
    EXPECT_CALL(*list_shortcuts_request_proxy, DoConnectToSignal(_, _, _, _))
        .WillOnce(Invoke(
            [&](const std::string& interface_name,
                const std::string& signal_name,
                dbus::ObjectProxy::SignalCallback signal_callback,
                dbus::ObjectProxy::OnConnectedCallback* on_connected_callback) {
              EXPECT_EQ(interface_name, "org.freedesktop.portal.Request");
              EXPECT_EQ(signal_name, "Response");

              std::move(*on_connected_callback)
                  .Run(interface_name, signal_name, true);

              dbus::Signal signal(interface_name, signal_name);
              dbus::MessageWriter writer(&signal);
              writer.AppendUint32(kResponseSuccess);
              // Simulate empty list of shortcuts
              MakeDbusDictionary("shortcuts", DbusShortcuts()).Write(&writer);
              signal_callback.Run(&signal);
            }));
    return list_shortcuts_request_proxy.get();
  };

  auto get_object_proxy_bind_shortcuts =
      [&](std::string_view service_name,
          const dbus::ObjectPath& object_path) -> dbus::ObjectProxy* {
    // BindShortcuts
    bind_shortcuts_request_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
        mock_bus.get(), GlobalShortcutListenerLinux::kPortalServiceName,
        object_path);
    EXPECT_CALL(*bind_shortcuts_request_proxy, DoConnectToSignal(_, _, _, _))
        .WillOnce(Invoke(
            [&](const std::string& interface_name,
                const std::string& signal_name,
                dbus::ObjectProxy::SignalCallback signal_callback,
                dbus::ObjectProxy::OnConnectedCallback* on_connected_callback) {
              EXPECT_EQ(interface_name, "org.freedesktop.portal.Request");
              EXPECT_EQ(signal_name, "Response");

              std::move(*on_connected_callback)
                  .Run(interface_name, signal_name, true);

              dbus::Signal signal(interface_name, signal_name);
              dbus::MessageWriter writer(&signal);
              writer.AppendUint32(kResponseSuccess);
              DbusDictionary().Write(&writer);
              signal_callback.Run(&signal);
            }));
    return bind_shortcuts_request_proxy.get();
  };

  EXPECT_CALL(
      *mock_bus,
      GetObjectProxy(GlobalShortcutListenerLinux::kPortalServiceName, _))
      .WillOnce(Invoke(get_object_proxy_session))
      .WillOnce(Invoke(get_object_proxy_create_session))
      .WillOnce(Invoke(get_object_proxy_list_shortcuts))
      .WillOnce(Invoke(get_object_proxy_bind_shortcuts));

  // CreateSession request
  EXPECT_CALL(
      *mock_global_shortcuts_proxy,
      DoCallMethod(
          MatchMethod(GlobalShortcutListenerLinux::kGlobalShortcutsInterface,
                      GlobalShortcutListenerLinux::kMethodCreateSession),
          _, _))
      .WillOnce(Invoke([&](dbus::MethodCall* method_call, int timeout_ms,
                           dbus::ObjectProxy::ResponseCallback* callback) {
        dbus::MessageReader reader(method_call);
        DbusDictionary options;
        EXPECT_TRUE(options.Read(&reader));
        auto* token = options.GetAs<DbusString>("session_handle_token");
        ASSERT_TRUE(token);
        std::string session_path_str =
            base::nix::XdgDesktopPortalSessionPath(kBusName, token->value());
        EXPECT_EQ(dbus::ObjectPath(session_path_str),
                  session_proxy->object_path());

        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendObjectPath(create_session_request_proxy->object_path());
        std::move(*callback).Run(response.get());
      }));

  // ListShortcuts request
  EXPECT_CALL(
      *mock_global_shortcuts_proxy,
      DoCallMethod(
          MatchMethod(GlobalShortcutListenerLinux::kGlobalShortcutsInterface,
                      GlobalShortcutListenerLinux::kMethodListShortcuts),
          _, _))
      .WillOnce(Invoke([&](dbus::MethodCall* method_call, int timeout_ms,
                           dbus::ObjectProxy::ResponseCallback* callback) {
        dbus::MessageReader reader(method_call);
        dbus::ObjectPath session_path;
        EXPECT_TRUE(reader.PopObjectPath(&session_path));
        DbusDictionary options;
        EXPECT_TRUE(options.Read(&reader));

        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendObjectPath(list_shortcuts_request_proxy->object_path());
        std::move(*callback).Run(response.get());
      }));

  // BindShortcuts request
  EXPECT_CALL(
      *mock_global_shortcuts_proxy,
      DoCallMethod(
          MatchMethod(GlobalShortcutListenerLinux::kGlobalShortcutsInterface,
                      GlobalShortcutListenerLinux::kMethodBindShortcuts),
          _, _))
      .WillOnce(Invoke([&](dbus::MethodCall* method_call, int timeout_ms,
                           dbus::ObjectProxy::ResponseCallback* callback) {
        dbus::MessageReader reader(method_call);
        dbus::ObjectPath session_path;
        EXPECT_TRUE(reader.PopObjectPath(&session_path));
        DbusShortcuts shortcuts;
        EXPECT_TRUE(shortcuts.Read(&reader));
        DbusString parent_window;
        EXPECT_TRUE(parent_window.Read(&reader));

        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendObjectPath(bind_shortcuts_request_proxy->object_path());
        std::move(*callback).Run(response.get());
      }));

  CommandMap commands;
  commands[kCommandName] = Command(kCommandName, kShortcutDescription,
                                   Command::AcceleratorToString(ui::Accelerator(
                                       ui::VKEY_A, ui::EF_CONTROL_DOWN)),
                                   /*global=*/true);

  global_shortcut_listener->OnCommandsChanged(kExtensionId, kProfileId,
                                              commands, observer.get());

  // Simulate the Activated signal
  EXPECT_CALL(*observer, ExecuteCommand(kExtensionId, kCommandName));
  dbus::Signal signal(GlobalShortcutListenerLinux::kGlobalShortcutsInterface,
                      GlobalShortcutListenerLinux::kSignalActivated);
  dbus::MessageWriter writer(&signal);
  writer.AppendObjectPath(session_proxy->object_path());
  writer.AppendString(kCommandName);
  writer.AppendUint64(0);  // timestamp
  activated_callback.Run(&signal);

  // Cleanup
  EXPECT_CALL(*session_proxy,
              DoCallMethod(
                  MatchMethod(GlobalShortcutListenerLinux::kSessionInterface,
                              GlobalShortcutListenerLinux::kMethodCloseSession),
                  _, _));
  EXPECT_CALL(*mock_bus, ShutdownAndBlock());
  global_shortcut_listener.reset();
}

}  // namespace extensions
