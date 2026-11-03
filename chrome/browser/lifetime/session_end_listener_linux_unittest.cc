// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/session_end_listener_linux.h"

#include <fcntl.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/nix/xdg_util.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/dbus/utils/read_value.h"
#include "components/dbus/utils/variant.h"
#include "components/dbus/utils/write_value.h"
#include "components/dbus/xdg/inhibit_session_monitor.h"
#include "components/dbus/xdg/portal.h"
#include "components/dbus/xdg/portal_constants.h"
#include "components/dbus/xdg/request.h"
#include "components/version_info/version_info.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrEq;

namespace {

constexpr const char kLogindServiceName[] = "org.freedesktop.login1";
constexpr const char kLogindObjectPath[] = "/org/freedesktop/login1";
constexpr const char kLogindManagerInterface[] =
    "org.freedesktop.login1.Manager";
constexpr const char kPrepareForShutdownSignal[] = "PrepareForShutdown";
constexpr const char kInhibitMethod[] = "Inhibit";

constexpr const char kGnomeSessionManagerServiceName[] =
    "org.gnome.SessionManager";
constexpr const char kGnomeSessionManagerObjectPath[] =
    "/org/gnome/SessionManager";
constexpr const char kGnomeSessionManagerInterface[] =
    "org.gnome.SessionManager";
constexpr const char kSessionOverSignal[] = "SessionOver";

constexpr const char kInhibitPortalInterface[] =
    "org.freedesktop.portal.Inhibit";
constexpr const char kStateChangedSignal[] = "StateChanged";

MATCHER_P(IsMethodCall, method, "") {
  return arg->GetMember() == method;
}

}  // namespace

class SessionEndListenerLinuxTest : public testing::Test {
 public:
  void SetUp() override {
    // Most tests don't exercise the Inhibit portal; make it unavailable.
    dbus_xdg::SetPortalStateForTesting(dbus_xdg::PortalRegistrarState::kFailed);
    mock_system_bus_ =
        base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());
    mock_session_bus_ =
        base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());

    mock_logind_proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        mock_system_bus_.get(), kLogindServiceName,
        dbus::ObjectPath(kLogindObjectPath));
    mock_gnome_session_proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        mock_session_bus_.get(), kGnomeSessionManagerServiceName,
        dbus::ObjectPath(kGnomeSessionManagerObjectPath));

    EXPECT_CALL(*mock_system_bus_,
                GetObjectProxy(StrEq(kLogindServiceName),
                               dbus::ObjectPath(kLogindObjectPath)))
        .WillRepeatedly(Return(mock_logind_proxy_.get()));

    EXPECT_CALL(
        *mock_session_bus_,
        GetObjectProxy(StrEq(kGnomeSessionManagerServiceName),
                       dbus::ObjectPath(kGnomeSessionManagerObjectPath)))
        .WillRepeatedly(Return(mock_gnome_session_proxy_.get()));
  }

  void TearDown() override {
    dbus_xdg::SetPortalStateForTesting(dbus_xdg::PortalRegistrarState::kIdle);
    mock_request_proxy_.reset();
    mock_portal_proxy_.reset();
    mock_logind_proxy_.reset();
    mock_gnome_session_proxy_.reset();
    mock_system_bus_.reset();
    mock_session_bus_.reset();
  }

 protected:
  void SetUpLogindSignal(bool connect_success) {
    EXPECT_CALL(*mock_logind_proxy_,
                ConnectToSignal(StrEq(kLogindManagerInterface),
                                StrEq(kPrepareForShutdownSignal), _, _))
        .WillOnce(
            [this, connect_success](
                const std::string& interface_name,
                const std::string& signal_name,
                dbus::ObjectProxy::SignalCallback signal_callback,
                dbus::ObjectProxy::OnConnectedCallback on_connected_callback) {
              logind_signal_callback_ = std::move(signal_callback);
              task_environment_.GetMainThreadTaskRunner()->PostTask(
                  FROM_HERE,
                  base::BindOnce(std::move(on_connected_callback),
                                 interface_name, signal_name, connect_success));
            });
  }

  void SetUpGnomeSignal(bool connect_success) {
    EXPECT_CALL(*mock_gnome_session_proxy_,
                ConnectToSignal(StrEq(kGnomeSessionManagerInterface),
                                StrEq(kSessionOverSignal), _, _))
        .WillOnce(
            [this, connect_success](
                const std::string& interface_name,
                const std::string& signal_name,
                dbus::ObjectProxy::SignalCallback signal_callback,
                dbus::ObjectProxy::OnConnectedCallback on_connected_callback) {
              gnome_signal_callback_ = std::move(signal_callback);
              task_environment_.GetMainThreadTaskRunner()->PostTask(
                  FROM_HERE,
                  base::BindOnce(std::move(on_connected_callback),
                                 interface_name, signal_name, connect_success));
            });
  }

  void SetUpInhibitCallSuccess() {
    EXPECT_CALL(*mock_logind_proxy_,
                CallMethodWithErrorResponse(IsMethodCall(kInhibitMethod), _, _))
        .WillOnce([this](dbus::MethodCall* method_call, int timeout_ms,
                         dbus::ObjectProxy::ResponseOrErrorCallback callback) {
          int pipe_fds[2];
          ASSERT_EQ(pipe(pipe_fds), 0);
          base::ScopedFD read_fd(pipe_fds[0]);
          base::ScopedFD write_fd(pipe_fds[1]);

          response_ = dbus::Response::CreateEmpty();
          dbus::MessageWriter writer(response_.get());
          writer.AppendFileDescriptor(read_fd.get());

          task_environment_.GetMainThreadTaskRunner()->PostTask(
              FROM_HERE,
              base::BindOnce(std::move(callback), response_.get(), nullptr));
        });
  }

  void SetUpInhibitCallFailure() {
    EXPECT_CALL(*mock_logind_proxy_,
                CallMethodWithErrorResponse(IsMethodCall(kInhibitMethod), _, _))
        .WillOnce([this](dbus::MethodCall* method_call, int timeout_ms,
                         dbus::ObjectProxy::ResponseOrErrorCallback callback) {
          dbus::MethodCall call(kLogindManagerInterface, kInhibitMethod);
          call.SetSerial(1);
          error_response_ = dbus::ErrorResponse::FromMethodCall(
              &call, "org.freedesktop.DBus.Error.AccessDenied", "Denied");
          task_environment_.GetMainThreadTaskRunner()->PostTask(
              FROM_HERE, base::BindOnce(std::move(callback), nullptr,
                                        error_response_.get()));
        });
  }

  void EmitPrepareForShutdown(bool is_preparing) {
    dbus::Signal signal(kLogindManagerInterface, kPrepareForShutdownSignal);
    dbus::MessageWriter writer(&signal);
    writer.AppendBool(is_preparing);
    logind_signal_callback_.Run(&signal);
  }

  void EmitSessionOver() {
    dbus::Signal signal(kGnomeSessionManagerInterface, kSessionOverSignal);
    gnome_signal_callback_.Run(&signal);
  }

  // Makes the Inhibit portal available on the session bus. Records the
  // StateChanged signal callback and the monitor session path from
  // CreateMonitor, and the paths passed to QueryEndResponse.
  void SetUpInhibitPortal() {
    dbus_xdg::SetPortalStateForTesting(
        dbus_xdg::PortalRegistrarState::kSuccess);
    mock_portal_proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        mock_session_bus_.get(), dbus_xdg::kPortalServiceName,
        dbus::ObjectPath(dbus_xdg::kPortalObjectPath));
    EXPECT_CALL(*mock_session_bus_, GetConnectionName())
        .WillRepeatedly(Return("test"));
    EXPECT_CALL(*mock_session_bus_,
                GetObjectProxy(StrEq(dbus_xdg::kPortalServiceName),
                               dbus::ObjectPath(dbus_xdg::kPortalObjectPath)))
        .WillRepeatedly(Return(mock_portal_proxy_.get()));
    // Request objects of the CreateMonitor call.
    mock_request_proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        mock_session_bus_.get(), dbus_xdg::kPortalServiceName,
        dbus::ObjectPath("/org/freedesktop/portal/desktop/request/1"));
    EXPECT_CALL(
        *mock_session_bus_,
        GetObjectProxy(StrEq(dbus_xdg::kPortalServiceName),
                       testing::Property(&dbus::ObjectPath::value,
                                         testing::HasSubstr("/request/"))))
        .WillRepeatedly(Return(mock_request_proxy_.get()));
    EXPECT_CALL(*mock_request_proxy_, ConnectToSignal(_, _, _, _))
        .WillRepeatedly(
            [](const std::string& interface_name,
               const std::string& signal_name,
               dbus::ObjectProxy::SignalCallback signal_callback,
               dbus::ObjectProxy::OnConnectedCallback on_connected) {
              std::move(on_connected).Run(interface_name, signal_name, true);
            });
    EXPECT_CALL(*mock_portal_proxy_,
                ConnectToSignal(StrEq(kInhibitPortalInterface),
                                StrEq(kStateChangedSignal), _, _))
        .WillOnce([this](const std::string& interface_name,
                         const std::string& signal_name,
                         dbus::ObjectProxy::SignalCallback signal_callback,
                         dbus::ObjectProxy::OnConnectedCallback on_connected) {
          state_changed_callback_ = std::move(signal_callback);
          std::move(on_connected).Run(interface_name, signal_name, true);
        });
    EXPECT_CALL(*mock_portal_proxy_, CallMethodWithErrorResponse(_, _, _))
        .WillRepeatedly([this](dbus::MethodCall* call, int timeout_ms,
                               dbus::ObjectProxy::ResponseOrErrorCallback cb) {
          dbus::MessageReader reader(call);
          if (call->GetMember() == "CreateMonitor") {
            std::string window;
            ASSERT_TRUE(reader.PopString(&window));
            auto options = dbus_utils::ReadValue<dbus_xdg::Dictionary>(reader);
            ASSERT_TRUE(options);
            auto it = options->find("session_handle_token");
            ASSERT_NE(it, options->end());
            auto token = std::move(it->second).Take<std::string>();
            ASSERT_TRUE(token);
            monitor_session_path_ = dbus::ObjectPath(
                base::nix::XdgDesktopPortalSessionPath("test", *token));
            // The Request::Response signal is not needed for these tests.
          } else if (call->GetMember() == "QueryEndResponse") {
            dbus::ObjectPath path;
            ASSERT_TRUE(reader.PopObjectPath(&path));
            query_end_responses_.push_back(path);
          }
        });
  }

  void EmitInhibitSessionState(
      dbus_xdg::InhibitSessionMonitor::SessionState state) {
    dbus::Signal signal(kInhibitPortalInterface, kStateChangedSignal);
    dbus::MessageWriter writer(&signal);
    writer.AppendObjectPath(monitor_session_path_);
    dbus_xdg::Dictionary dict;
    dict["session-state"] =
        dbus_utils::Variant::Wrap<"u">(static_cast<uint32_t>(state));
    dbus_utils::WriteValue(writer, dict);
    state_changed_callback_.Run(&signal);
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  scoped_refptr<dbus::MockBus> mock_system_bus_;
  scoped_refptr<dbus::MockBus> mock_session_bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_logind_proxy_;
  scoped_refptr<dbus::MockObjectProxy> mock_gnome_session_proxy_;
  scoped_refptr<dbus::MockObjectProxy> mock_portal_proxy_;
  scoped_refptr<dbus::MockObjectProxy> mock_request_proxy_;

  std::unique_ptr<dbus::Response> response_;
  std::unique_ptr<dbus::ErrorResponse> error_response_;

  dbus::ObjectProxy::SignalCallback logind_signal_callback_;
  dbus::ObjectProxy::SignalCallback gnome_signal_callback_;
  dbus::ObjectProxy::SignalCallback state_changed_callback_;
  dbus::ObjectPath monitor_session_path_;
  std::vector<dbus::ObjectPath> query_end_responses_;
};

TEST_F(SessionEndListenerLinuxTest, LogindShutdownFlow) {
  SetUpLogindSignal(true);
  SetUpGnomeSignal(true);
  SetUpInhibitCallSuccess();

  base::test::TestFuture<void> session_ended;
  auto listener = std::make_unique<SessionEndListenerLinux>(
      mock_system_bus_, mock_session_bus_, session_ended.GetCallback());

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(listener->has_inhibitor_for_testing());

  // Simulate PrepareForShutdown(true).
  EmitPrepareForShutdown(true);
  EXPECT_TRUE(session_ended.Wait());
}

TEST_F(SessionEndListenerLinuxTest, GnomeSessionOverFlow) {
  SetUpLogindSignal(true);
  SetUpGnomeSignal(true);
  SetUpInhibitCallSuccess();

  base::test::TestFuture<void> session_ended;
  auto listener = std::make_unique<SessionEndListenerLinux>(
      mock_system_bus_, mock_session_bus_, session_ended.GetCallback());

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(listener->has_inhibitor_for_testing());

  // Simulate SessionOver.
  EmitSessionOver();
  EXPECT_TRUE(session_ended.Wait());
}

TEST_F(SessionEndListenerLinuxTest, LogindSignalConnectFailed) {
  SetUpLogindSignal(false);
  SetUpGnomeSignal(true);

  // Inhibit should not be called if signal connection fails.
  EXPECT_CALL(*mock_logind_proxy_, CallMethodWithErrorResponse(_, _, _))
      .Times(0);

  base::test::TestFuture<void> session_ended;
  auto listener = std::make_unique<SessionEndListenerLinux>(
      mock_system_bus_, mock_session_bus_, session_ended.GetCallback());

  task_environment_.RunUntilIdle();
  EXPECT_FALSE(listener->has_inhibitor_for_testing());
}

TEST_F(SessionEndListenerLinuxTest, LogindInhibitFailed) {
  SetUpLogindSignal(true);
  SetUpGnomeSignal(true);
  SetUpInhibitCallFailure();

  base::test::TestFuture<void> session_ended;
  auto listener = std::make_unique<SessionEndListenerLinux>(
      mock_system_bus_, mock_session_bus_, session_ended.GetCallback());

  task_environment_.RunUntilIdle();
  EXPECT_FALSE(listener->has_inhibitor_for_testing());
}

TEST_F(SessionEndListenerLinuxTest, ShutdownCancelledReacquiresInhibitor) {
  SetUpLogindSignal(true);
  SetUpGnomeSignal(true);
  SetUpInhibitCallSuccess();

  base::test::TestFuture<void> session_ended;
  auto listener = std::make_unique<SessionEndListenerLinux>(
      mock_system_bus_, mock_session_bus_, session_ended.GetCallback());

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(listener->has_inhibitor_for_testing());

  // Inhibit call should be set up for when shutdown is cancelled.
  SetUpInhibitCallSuccess();

  // Simulate PrepareForShutdown(false) when no inhibitor is held.
  // First clear inhibitor to simulate post-release cancellation.
  listener->reset_inhibitor_for_testing();
  EXPECT_FALSE(listener->has_inhibitor_for_testing());

  EmitPrepareForShutdown(false);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(listener->has_inhibitor_for_testing());
  EXPECT_FALSE(session_ended.IsReady());
}

TEST_F(SessionEndListenerLinuxTest, InhibitPortalSessionEnding) {
  SetUpLogindSignal(true);
  SetUpGnomeSignal(true);
  SetUpInhibitCallSuccess();
  SetUpInhibitPortal();

  base::test::TestFuture<void> session_ended;
  auto listener = std::make_unique<SessionEndListenerLinux>(
      mock_system_bus_, mock_session_bus_, session_ended.GetCallback());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(listener->has_inhibit_session_monitor_for_testing());
  ASSERT_TRUE(monitor_session_path_.IsValid());

  // "Query End" is acknowledged, but logout may still be cancelled.
  EmitInhibitSessionState(
      dbus_xdg::InhibitSessionMonitor::SessionState::kQueryEnd);
  task_environment_.RunUntilIdle();
  ASSERT_EQ(query_end_responses_.size(), 1u);
  EXPECT_EQ(query_end_responses_[0], monitor_session_path_);
  EXPECT_FALSE(session_ended.IsReady());

  EmitInhibitSessionState(
      dbus_xdg::InhibitSessionMonitor::SessionState::kEnding);
  EXPECT_TRUE(session_ended.Wait());
}

TEST_F(SessionEndListenerLinuxTest, InhibitPortalLogoutCancelled) {
  SetUpLogindSignal(true);
  SetUpGnomeSignal(true);
  SetUpInhibitCallSuccess();
  SetUpInhibitPortal();

  base::test::TestFuture<void> session_ended;
  auto listener = std::make_unique<SessionEndListenerLinux>(
      mock_system_bus_, mock_session_bus_, session_ended.GetCallback());
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(monitor_session_path_.IsValid());

  EmitInhibitSessionState(
      dbus_xdg::InhibitSessionMonitor::SessionState::kQueryEnd);
  EmitInhibitSessionState(
      dbus_xdg::InhibitSessionMonitor::SessionState::kRunning);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(query_end_responses_.size(), 1u);
  EXPECT_FALSE(session_ended.IsReady());
}

TEST_F(SessionEndListenerLinuxTest, InhibitPortalFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kLinuxInhibitPortalSessionMonitor);
  SetUpLogindSignal(true);
  SetUpGnomeSignal(true);
  SetUpInhibitCallSuccess();

  auto listener = std::make_unique<SessionEndListenerLinux>(
      mock_system_bus_, mock_session_bus_, base::DoNothing());
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(listener->has_inhibit_session_monitor_for_testing());
}

TEST_F(SessionEndListenerLinuxTest, LogindNotUsedInSnap) {
  base::nix::ScopedAppSandboxOverrideForTesting snap(
      base::nix::AppSandbox::kSnap);
  SetUpGnomeSignal(true);
  SetUpInhibitPortal();

  // Snap's AppArmor policy denies all access to logind, so neither the
  // PrepareForShutdown signal nor the Inhibit method should be used.
  EXPECT_CALL(*mock_system_bus_,
              GetObjectProxy(StrEq(kLogindServiceName),
                             dbus::ObjectPath(kLogindObjectPath)))
      .Times(0);
  EXPECT_CALL(*mock_logind_proxy_, ConnectToSignal(_, _, _, _)).Times(0);
  EXPECT_CALL(*mock_logind_proxy_, CallMethodWithErrorResponse(_, _, _))
      .Times(0);

  base::test::TestFuture<void> session_ended;
  auto listener = std::make_unique<SessionEndListenerLinux>(
      mock_system_bus_, mock_session_bus_, session_ended.GetCallback());

  // The Inhibit portal still reports the end of the session.
  EXPECT_TRUE(listener->has_inhibit_session_monitor_for_testing());
  ASSERT_TRUE(
      base::test::RunUntil([&] { return monitor_session_path_.IsValid(); }));
  EmitInhibitSessionState(
      dbus_xdg::InhibitSessionMonitor::SessionState::kEnding);
  EXPECT_TRUE(session_ended.Wait());
  EXPECT_FALSE(listener->has_inhibitor_for_testing());
}
