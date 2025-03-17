// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRACING_WINDOWS_SYSTEM_TRACING_CLIENT_IMPL_WIN_H_
#define CHROME_BROWSER_TRACING_WINDOWS_SYSTEM_TRACING_CLIENT_IMPL_WIN_H_

#include <wrl/client.h>

#include <utility>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_handle.h"
#include "base/threading/sequence_bound.h"
#include "base/types/expected.h"
#include "base/win/windows_types.h"
#include "chrome/browser/tracing/windows_system_tracing_client_win.h"
#include "chrome/windows_services/elevated_tracing_service/tracing_service_idl.h"
#include "mojo/public/cpp/system/message_pipe.h"

class WindowsSystemTracingClientImpl : public WindowsSystemTracingClient {
 public:
  WindowsSystemTracingClientImpl(const CLSID& clsid, const IID& iid);
  WindowsSystemTracingClientImpl(const WindowsSystemTracingClientImpl&) =
      delete;
  WindowsSystemTracingClientImpl& operator=(
      const WindowsSystemTracingClientImpl&) = delete;
  ~WindowsSystemTracingClientImpl() override;

  void Start(OnRemoteProcess on_remote_process) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(WindowsSystemTracingClientImplTest, InviteSucceeds);
  FRIEND_TEST_ALL_PREFIXES(WindowsSystemTracingClientImplTest, AcceptFails);
  FRIEND_TEST_ALL_PREFIXES(WindowsSystemTracingClientImplTest, LaunchFailed);
  FRIEND_TEST_ALL_PREFIXES(WindowsSystemTracingClientImplTest, RunsCallback);

  using ServiceState =
      std::pair<base::ProcessId, mojo::ScopedMessagePipeHandle>;

  // Lives on a STA COM thread.
  class Host {
   public:
    Host(const CLSID& clsid, const IID& iid);
    Host(const Host&) = delete;
    Host& operator=(const Host&) = delete;
    ~Host();

    // Launches the system tracing service. Returns the service's pid on
    // success, or a failure reason in case of failure.
    base::expected<ServiceState, HRESULT> LaunchService();

    // Creates the trace session object. Public for ease of testing.
    base::expected<Microsoft::WRL::ComPtr<ISystemTraceSession>, HRESULT>
    CreateSession();

    // Sends the mojo invitation to the service. Public for ease of testing.
    base::expected<ServiceState, HRESULT> Invite(
        Microsoft::WRL::ComPtr<ISystemTraceSession> trace_session);

   private:
    // The system tracing service's class and interface ids.
    const CLSID clsid_;
    const IID iid_;

    // The trace session in the system tracing service.
    Microsoft::WRL::ComPtr<ISystemTraceSession> trace_session_;
  };

  // Handles the result of launching the service. On success, the process is
  // registered as a client process with the tracing system.
  void OnLaunchServiceResult(OnRemoteProcess on_remote_process,
                             base::expected<ServiceState, HRESULT> result);

  // The host, responsible for COM interactions with the system tracing service.
  base::SequenceBound<Host> host_;

  base::WeakPtrFactory<WindowsSystemTracingClientImpl> weak_factory_{this};
};

#endif  // CHROME_BROWSER_TRACING_WINDOWS_SYSTEM_TRACING_CLIENT_IMPL_WIN_H_
