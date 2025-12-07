// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tracing/windows_system_tracing_client_impl_win.h"

#include <objbase.h>

#include <stdint.h>

#include <ios>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/thread_pool.h"
#include "base/win/com_init_util.h"
#include "base/win/win_util.h"
#include "chrome/installer/util/install_service_work_item.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"

namespace {

enum class LaunchStage {
  kCoCreateInstance,
  kCoSetProxyBlanket,
  kAcceptInvitation,
};

// Records the result of one stage while launching the elevated tracing service.
void RecordLaunchResult(LaunchStage stage, HRESULT result) {
  std::string_view stage_name;
  switch (stage) {
    case LaunchStage::kCoCreateInstance:
      stage_name = "CoCreateInstance";
      break;
    case LaunchStage::kCoSetProxyBlanket:
      stage_name = "CoSetProxyBlanket";
      break;
    case LaunchStage::kAcceptInvitation:
      stage_name = "AcceptInvitation";
      break;
  }
  base::UmaHistogramSparse(
      base::StrCat(
          {"Tracing.ElevatedTracingService.LaunchResult.", stage_name}),
      result);
}

}  // namespace

// WindowsSystemTracingClientImpl::Host ----------------------------------------

WindowsSystemTracingClientImpl::Host::Host(const CLSID& clsid, const IID& iid)
    : clsid_(clsid), iid_(iid) {}

WindowsSystemTracingClientImpl::Host::~Host() = default;

base::expected<WindowsSystemTracingClientImpl::ServiceState, HRESULT>
WindowsSystemTracingClientImpl::Host::LaunchService() {
  base::win::AssertComInitialized();

  if (auto session_or_result = CreateSession(); session_or_result.has_value()) {
    return Invite(std::move(session_or_result).value());
  } else {
    return base::unexpected(session_or_result.error());
  }
}
base::expected<Microsoft::WRL::ComPtr<ISystemTraceSession>, HRESULT>
WindowsSystemTracingClientImpl::Host::CreateSession() {
  Microsoft::WRL::ComPtr<ISystemTraceSession> trace_session;
  HRESULT hresult = ::CoCreateInstance(
      clsid_, /*pUnkOuter=*/nullptr, CLSCTX_LOCAL_SERVER, iid_, &trace_session);
  // Do not record failure to create the instance if the service isn't
  // registered. This is expected for per-machine beta and stable installs for
  // which the user has not manually registered the service via
  // chrome://traces-internals/scenarios.
  if (hresult != REGDB_E_CLASSNOTREG ||
      installer::InstallServiceWorkItem::IsComServiceInstalled(clsid_)) {
    RecordLaunchResult(LaunchStage::kCoCreateInstance, hresult);
  }
  if (FAILED(hresult)) {
    return base::unexpected(hresult);
  }

  hresult = ::CoSetProxyBlanket(trace_session.Get(), RPC_C_AUTHN_DEFAULT,
                                RPC_C_AUTHZ_DEFAULT, COLE_DEFAULT_PRINCIPAL,
                                RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
                                RPC_C_IMP_LEVEL_IMPERSONATE,
                                /*pAuthInfo=*/nullptr, EOAC_DYNAMIC_CLOAKING);
  RecordLaunchResult(LaunchStage::kCoSetProxyBlanket, hresult);
  if (FAILED(hresult)) {
    return base::unexpected(hresult);
  }
  return base::ok(std::move(trace_session));
}

base::expected<WindowsSystemTracingClientImpl::ServiceState, HRESULT>
WindowsSystemTracingClientImpl::Host::Invite(
    Microsoft::WRL::ComPtr<ISystemTraceSession> trace_session) {
  mojo::NamedPlatformChannel channel(mojo::NamedPlatformChannel::Options{});
  mojo::OutgoingInvitation invitation;
  invitation.set_extra_flags(MOJO_SEND_INVITATION_FLAG_ELEVATED);
  mojo::ScopedMessagePipeHandle pipe = invitation.AttachMessagePipe(0);

  DWORD pid = base::kNullProcessId;
  HRESULT hresult =
      trace_session->AcceptInvitation(channel.GetServerName().c_str(), &pid);
  RecordLaunchResult(LaunchStage::kAcceptInvitation, hresult);
  if (FAILED(hresult)) {
    return base::unexpected(hresult);
  }

  mojo::OutgoingInvitation::Send(std::move(invitation),
                                 /*target_process=*/base::kNullProcessHandle,
                                 channel.TakeServerEndpoint());

  trace_session_ = std::move(trace_session);
  return base::ok(std::make_pair(pid, std::move(pipe)));
}

// WindowsSystemTracingClientImpl ----------------------------------------------

WindowsSystemTracingClientImpl::WindowsSystemTracingClientImpl(
    const CLSID& clsid,
    const IID& iid)
    : host_(base::ThreadPool::CreateCOMSTATaskRunner(
                {base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN,
                 base::MayBlock()}),
            clsid,
            iid) {}

WindowsSystemTracingClientImpl::~WindowsSystemTracingClientImpl() = default;

void WindowsSystemTracingClientImpl::Start(OnRemoteProcess on_remote_process) {
  host_.AsyncCall(&Host::LaunchService)
      .Then(base::BindOnce(
          &WindowsSystemTracingClientImpl::OnLaunchServiceResult,
          weak_factory_.GetWeakPtr(), std::move(on_remote_process)));
}

void WindowsSystemTracingClientImpl::OnLaunchServiceResult(
    OnRemoteProcess on_remote_process,
    base::expected<ServiceState, HRESULT> result) {
  if (!result.has_value()) {
    return;
  }
  std::move(on_remote_process)
      .Run(result->first, mojo::PendingRemote<tracing::mojom::TracedProcess>(
                              std::move(result->second), /*version=*/0));
}

// WindowsSystemTracingClient --------------------------------------------------

// static
std::unique_ptr<WindowsSystemTracingClient> WindowsSystemTracingClient::Create(
    const CLSID& clsid,
    const IID& iid) {
  return std::make_unique<WindowsSystemTracingClientImpl>(clsid, iid);
}
