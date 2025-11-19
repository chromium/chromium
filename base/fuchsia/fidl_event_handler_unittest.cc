// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/fidl_event_handler.h"

#include <fidl/base.testfidl/cpp/fidl.h>
#include <fidl/fuchsia.logger/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>

#include "base/fuchsia/fuchsia_component_connect.h"
#include "base/fuchsia/process_context.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/fuchsia/test_interface_natural_impl.h"
#include "base/fuchsia/test_log_listener_safe.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/scoped_logging_settings.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kBaseUnittestsExec[] = "base_unittests__exec";

}  // namespace

namespace base {

class FidlEventHandlerTest : public testing::Test {
 public:
  FidlEventHandlerTest() {
    test_context_.AddService(
        fidl::DiscoverableProtocolName<fuchsia_logger::Log>);
    ListenFilteredByCurrentProcessId(listener_);
    // Initialize logging in the `scoped_logging_settings_`.
    CHECK(logging::InitLogging({.logging_dest = logging::LOG_DEFAULT}));
  }
  FidlEventHandlerTest(const FidlEventHandlerTest&) = delete;
  FidlEventHandlerTest& operator=(const FidlEventHandlerTest&) = delete;
  ~FidlEventHandlerTest() override = default;

 protected:
  test::SingleThreadTaskEnvironment task_environment_{
      test::SingleThreadTaskEnvironment::MainThreadType::IO};
  SimpleTestLogListener listener_;

  // Ensure that logging is directed to the system debug log.
  logging::ScopedLoggingSettings scoped_logging_settings_;
  TestComponentContextForProcess test_context_;
  TestInterfaceNaturalImpl test_service_;
};

TEST_F(FidlEventHandlerTest, FidlErrorEventLogger) {
  FidlErrorEventLogger<base_testfidl::TestInterface> event_handler;

  event_handler.on_fidl_error(fidl::UnbindInfo::PeerClosed(ZX_ERR_PEER_CLOSED));

  constexpr char kLogMessage[] =
      "base.testfidl.TestInterface was disconnected with ZX_ERR_PEER_CLOSED";
  std::optional<fuchsia_logger::LogMessage> logged_message =
      listener_.RunUntilMessageReceived(kLogMessage);

  ASSERT_TRUE(logged_message.has_value());
  EXPECT_EQ(logged_message->severity(),
            static_cast<int32_t>(fuchsia_logger::LogLevelFilter::kError));
  ASSERT_EQ(logged_message->tags().size(), 1u);
  EXPECT_EQ(logged_message->tags()[0], kBaseUnittestsExec);
}

TEST_F(FidlEventHandlerTest, FidlErrorEventLogger_CustomProtocolName) {
  FidlErrorEventLogger<base_testfidl::TestInterface> event_handler(
      "test_protocol_name");

  event_handler.on_fidl_error(fidl::UnbindInfo::PeerClosed(ZX_ERR_PEER_CLOSED));

  constexpr char kLogMessage[] =
      "test_protocol_name was disconnected with ZX_ERR_PEER_CLOSED";
  std::optional<fuchsia_logger::LogMessage> logged_message =
      listener_.RunUntilMessageReceived(kLogMessage);

  ASSERT_TRUE(logged_message.has_value());
  EXPECT_EQ(logged_message->severity(),
            static_cast<int32_t>(fuchsia_logger::LogLevelFilter::kError));
  ASSERT_EQ(logged_message->tags().size(), 1u);
  EXPECT_EQ(logged_message->tags()[0], kBaseUnittestsExec);
}

TEST_F(FidlEventHandlerTest, FidlErrorEventLogger_LogsOnServiceClosure) {
  FidlErrorEventLogger<base_testfidl::TestInterface> event_handler;
  auto client_end = fuchsia_component::ConnectAt<base_testfidl::TestInterface>(
      test_context_.published_services_natural());
  EXPECT_TRUE(client_end.is_ok());
  fidl::Client client(std::move(*client_end), async_get_default_dispatcher(),
                      &event_handler);

  {
    ScopedNaturalServiceBinding<base_testfidl::TestInterface> binding(
        ComponentContextForProcess()->outgoing().get(), &test_service_);

    ASSERT_EQ(ZX_OK, VerifyTestInterface(client));
  };

  constexpr char kLogMessage[] =
      "base.testfidl.TestInterface was disconnected with ZX_ERR_PEER_CLOSED";
  std::optional<fuchsia_logger::LogMessage> logged_message =
      listener_.RunUntilMessageReceived(kLogMessage);

  ASSERT_TRUE(logged_message.has_value());
  EXPECT_EQ(logged_message->severity(),
            static_cast<int32_t>(fuchsia_logger::LogLevelFilter::kError));
  ASSERT_EQ(logged_message->tags().size(), 1u);
  EXPECT_EQ(logged_message->tags()[0], kBaseUnittestsExec);
}

TEST(FidlEventHandlerDeathTest, FidlErrorEventProcessExiter) {
  FidlErrorEventProcessExiter<base_testfidl::TestInterface> event_handler;

  EXPECT_DEATH(
      event_handler.on_fidl_error(
          fidl::UnbindInfo::PeerClosed(ZX_ERR_PEER_CLOSED)),
      testing::HasSubstr("base.testfidl.TestInterface disconnected "
                         "unexpectedly, exiting: ZX_ERR_PEER_CLOSED (-24)"));
}

TEST(FidlEventHandlerDeathTest,
     FidlErrorEventProcessExiter_CustomProtocolName) {
  FidlErrorEventProcessExiter<base_testfidl::TestInterface> event_handler(
      "test_protocol_name");

  EXPECT_DEATH(
      event_handler.on_fidl_error(
          fidl::UnbindInfo::PeerClosed(ZX_ERR_PEER_CLOSED)),
      testing::HasSubstr("test_protocol_name disconnected unexpectedly, "
                         "exiting: ZX_ERR_PEER_CLOSED (-24)"));
}

TEST(FidlEventHandlerDeathTest,
     FidlErrorEventProcessExiter_LogsOnServiceClosure) {
  test::SingleThreadTaskEnvironment task_environment_{
      test::SingleThreadTaskEnvironment::MainThreadType::IO};
  TestComponentContextForProcess test_context;
  FidlErrorEventProcessExiter<base_testfidl::TestInterface> event_handler;
  auto client_end = fuchsia_component::ConnectAt<base_testfidl::TestInterface>(
      test_context.published_services_natural());
  EXPECT_TRUE(client_end.is_ok());
  fidl::Client client(std::move(*client_end), async_get_default_dispatcher(),
                      &event_handler);

  auto bind_and_close_service = [&] {
    {
      TestInterfaceNaturalImpl test_service;
      ScopedNaturalServiceBinding<base_testfidl::TestInterface> binding(
          ComponentContextForProcess()->outgoing().get(), &test_service);

      ASSERT_EQ(ZX_OK, VerifyTestInterface(client));
    }
    base::RunLoop().RunUntilIdle();
  };

  EXPECT_DEATH(
      bind_and_close_service(),
      testing::HasSubstr("base.testfidl.TestInterface disconnected "
                         "unexpectedly, exiting: ZX_ERR_PEER_CLOSED (-24)"));
}

TEST_F(FidlEventHandlerTest, FidlErrorEventHandler) {
  RunLoop loop;
  FidlErrorEventHandler<base_testfidl::TestInterface> event_handler(
      base::BindLambdaForTesting(
          [quit_closure = loop.QuitClosure()](fidl::UnbindInfo error) {
            ASSERT_TRUE(error.is_peer_closed());
            quit_closure.Run();
          }));

  event_handler.on_fidl_error(fidl::UnbindInfo::PeerClosed(ZX_ERR_PEER_CLOSED));

  loop.Run();
}

TEST_F(FidlEventHandlerTest, FidlErrorEventHandler_FiresOnServiceClosure) {
  RunLoop loop;
  FidlErrorEventHandler<base_testfidl::TestInterface> event_handler(
      base::BindLambdaForTesting(
          [quit_closure = loop.QuitClosure()](fidl::UnbindInfo error) {
            ASSERT_TRUE(error.is_peer_closed());
            quit_closure.Run();
          }));

  auto client_end = fuchsia_component::ConnectAt<base_testfidl::TestInterface>(
      test_context_.published_services_natural());
  EXPECT_TRUE(client_end.is_ok());
  fidl::Client client(std::move(*client_end), async_get_default_dispatcher(),
                      &event_handler);

  {
    ScopedNaturalServiceBinding<base_testfidl::TestInterface> binding(
        ComponentContextForProcess()->outgoing().get(), &test_service_);

    ASSERT_EQ(ZX_OK, VerifyTestInterface(client));
  };

  loop.Run();
}

}  // namespace base
