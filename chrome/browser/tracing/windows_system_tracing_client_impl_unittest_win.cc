// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "chrome/browser/tracing/windows_system_tracing_client_impl_win.h"
#include "chrome/windows_services/elevated_tracing_service/tracing_service_idl.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;

namespace {

constexpr CLSID kTestClsid = {0xfe929514,
                              0xe621,
                              0x4a08,
                              {0x95, 0x7b, 0x04, 0xa7, 0x38, 0x32, 0x03, 0x78}};

class MockSystemTraceSession : public ISystemTraceSession {
 public:
  MOCK_METHOD(HRESULT,
              AcceptInvitation,
              (const wchar_t* server_name, DWORD* pid),
              (override, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT,
              QueryInterface,
              (REFIID riid, void** ppvObject),
              (override, Calltype(STDMETHODCALLTYPE)));
  ULONG STDMETHODCALLTYPE AddRef() override { return ++ref_count_; }
  ULONG STDMETHODCALLTYPE Release() override { return --ref_count_; }

  ULONG ref_count() const { return ref_count_; }

 private:
  ULONG ref_count_ = 0;
};

}  // namespace

class WindowsSystemTracingClientImplTest : public ::testing::Test {
 protected:
  WindowsSystemTracingClientImplTest() = default;

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  mojo::core::ScopedIPCSupport ipc_support_{
      task_environment_.GetMainThreadTaskRunner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST};
};

// Tests that the factory function returns an instance.
TEST_F(WindowsSystemTracingClientImplTest, CreateWorks) {
  ASSERT_TRUE(
      WindowsSystemTracingClient::Create(kTestClsid, IID_ISystemTraceSession));
}

// Tests that the host sends a valid channel handle to the service on success.
TEST_F(WindowsSystemTracingClientImplTest, InviteSucceeds) {
  base::HistogramTester histogram_tester;
  StrictMock<MockSystemTraceSession> session;

  // Configure a mock tracing session that will return the current process's PID
  // and capture the server name to the channel on which the invitation will be
  // sent.
  mojo::NamedPlatformChannel::ServerName server_name;
  EXPECT_CALL(session, AcceptInvitation(_, _))
      .WillOnce([&server_name](const wchar_t* name, DWORD* pid) {
        server_name = name;
        *pid = ::GetCurrentProcessId();
        return S_OK;
      });

  WindowsSystemTracingClientImpl::Host host(kTestClsid,
                                            IID_ISystemTraceSession);

  ASSERT_OK_AND_ASSIGN(
      (auto [pid, message_pipe_handle]),
      host.Invite(Microsoft::WRL::ComPtr<ISystemTraceSession>(&session)));
  // The host should return the pid of the current process.
  ASSERT_EQ(pid, base::Process::Current().Pid());
  // The host should return a valid message pipe handle.
  ASSERT_TRUE(message_pipe_handle.is_valid());
  // The host should have held a reference to the service.
  ASSERT_EQ(session.ref_count(), 1U);
  // The host should have recorded success in the proper histogram.
  histogram_tester.ExpectUniqueSample(
      "Tracing.ElevatedTracingService.LaunchResult.AcceptInvitation", S_OK, 1);

  // The host should have given the service a valid server name.
  ASSERT_FALSE(server_name.empty());
}

// Tests that the host properly handles the case where the service reports a
// failure from `AcceptInvitation()`.
TEST_F(WindowsSystemTracingClientImplTest, AcceptFails) {
  base::HistogramTester histogram_tester;
  StrictMock<MockSystemTraceSession> session;

  EXPECT_CALL(session, AcceptInvitation(_, _)).WillOnce(Return(E_FAIL));

  WindowsSystemTracingClientImpl::Host host(kTestClsid,
                                            IID_ISystemTraceSession);

  auto service_state =
      host.Invite(Microsoft::WRL::ComPtr<ISystemTraceSession>(&session));
  ASSERT_FALSE(service_state.has_value());
  ASSERT_EQ(service_state.error(), E_FAIL);
  ASSERT_EQ(session.ref_count(), 0U);
  // The host should have recorded failure in the proper histogram.
  histogram_tester.ExpectUniqueSample(
      "Tracing.ElevatedTracingService.LaunchResult.AcceptInvitation", E_FAIL,
      1);
}

// Tests that nothing bad happens if launching the service fails.
TEST_F(WindowsSystemTracingClientImplTest, LaunchFailed) {
  WindowsSystemTracingClientImpl impl(kTestClsid, IID_ISystemTraceSession);

  StrictMock<base::MockCallback<WindowsSystemTracingClient::OnRemoteProcess>>
      callback;
  impl.OnLaunchServiceResult(callback.Get(), base::unexpected(E_FAIL));
}

// Tests that the callback is run when the launch succeeds.
TEST_F(WindowsSystemTracingClientImplTest, RunsCallback) {
  WindowsSystemTracingClientImpl impl(kTestClsid, IID_ISystemTraceSession);

  constexpr base::ProcessId kTestPid = 1234;

  StrictMock<base::MockCallback<WindowsSystemTracingClient::OnRemoteProcess>>
      callback;
  EXPECT_CALL(callback, Run(kTestPid, _));
  impl.OnLaunchServiceResult(
      callback.Get(),
      base::ok(std::make_pair(kTestPid, mojo::ScopedMessagePipeHandle())));
}
