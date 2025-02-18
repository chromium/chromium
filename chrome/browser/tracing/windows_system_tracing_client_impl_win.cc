// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tracing/windows_system_tracing_client_impl_win.h"

#include <objbase.h>

#include <stdint.h>

#include <ios>
#include <memory>
#include <utility>

#include "base/debug/alias.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/environment.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/win/com_init_util.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "chrome/common/env_vars.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/install_service_work_item.h"
#include "chrome/installer/util/install_util.h"
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

// Upload a crash dump for the first failure to start the service to help
// diagnose launch failures; see https://crbug.com/392441188.
void ReportFirstStartFailure(const CLSID& clsid, HRESULT result) {
  [[maybe_unused]] static const bool dump_once = [result, &clsid]() {
    // A helper function that reads the REG_SZ value named `name` from the
    // registry key `root\path` into `value`. `key_result` and `value_result`
    // are the result codes from RegOpenKeyEx() and RegQueryValueEx(),
    // respectively.
    static constexpr auto kReadString =
        [](HKEY root, const wchar_t* path, const wchar_t* name,
           LONG& key_result, LONG& value_result, std::wstring& value) {
          base::win::RegKey key;
          if (key_result = key.Open(root, path, KEY_QUERY_VALUE);
              key_result == ERROR_SUCCESS) {
            value_result = key.ReadValue(name, &value);
          }
        };

    // Suppress the report when running within automated test infrastructure.
    if (base::Environment::Create()->HasVar(env_vars::kHeadless)) {
      return true;
    }

    // Stringify the clsid.
    std::wstring clsid_string = base::win::WStringFromGUID(clsid);

    // Read the `AppId` value in the class's key.
    LONG class_key_result = ERROR_SUCCESS;
    LONG appid_value_result = ERROR_SUCCESS;
    std::wstring app_id;
    if (!clsid_string.empty()) {
      kReadString(HKEY_CLASSES_ROOT,
                  base::StrCat({L"CLSID\\", clsid_string}).c_str(), L"AppID",
                  class_key_result, appid_value_result, app_id);
    }

    // Read the `LocalService` value in the app id's key.
    LONG app_id_key_result = ERROR_SUCCESS;
    LONG local_service_value_result = ERROR_SUCCESS;
    std::wstring local_service;
    if (!app_id.empty()) {
      kReadString(HKEY_CLASSES_ROOT, base::StrCat({L"AppId\\", app_id}).c_str(),
                  L"LocalService", app_id_key_result,
                  local_service_value_result, local_service);
    }

    // Get the name of the service registered with the SCM.
    std::wstring service_name =
        installer::InstallServiceWorkItem::GetCurrentServiceName(
            install_static::GetTracingServiceName(),
            install_static::GetClientStateKeyPath());

    // Read the `ImagePath` value in the service's key.
    std::wstring service_path;
    LONG service_key_result = ERROR_SUCCESS;
    LONG image_path_value_result = ERROR_SUCCESS;
    std::wstring image_path;
    if (!local_service.empty()) {
      kReadString(HKEY_LOCAL_MACHINE,
                  base::StrCat(
                      {L"SYSTEM\\CurrentControlSet\\Services\\", local_service})
                      .c_str(),
                  L"ImagePath", service_key_result, image_path_value_result,
                  image_path);
    }

    // See if the service executable is present.
    base::FilePath exe_path;
    WIN32_FILE_ATTRIBUTE_DATA info = {};
    LONG get_attributes_result = ERROR_SUCCESS;
    if (!image_path.empty()) {
      base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
      cmd_line.ParseFromString(image_path);
      exe_path = cmd_line.GetProgram();

      if (!::GetFileAttributesExW(exe_path.value().c_str(),
                                  ::GetFileExInfoStandard, &info)) {
        get_attributes_result = ::GetLastError();
      }
    }

    // The HRESULT from CoCreateInstance.
    SCOPED_CRASH_KEY_NUMBER("tracing", "result", result);
    // The CLSID of the elevated tracing service. For dev channel, this should
    // be "{48C5C553-20F9-4CDC-8340-8529AB83C552}".
    SCOPED_CRASH_KEY_STRING64("tracing", "clsid",
                              base::WideToASCII(clsid_string));
    // The result of opening the class registration key; should be 0.
    SCOPED_CRASH_KEY_NUMBER("tracing", "class_key_result", class_key_result);
    // The result of reading AppId from the class registration key; should be 0.
    SCOPED_CRASH_KEY_NUMBER("tracing", "appid_value_result",
                            appid_value_result);
    // The class's AppId; should be "{48C5C553-20F9-4CDC-8340-8529AB83C552}".
    SCOPED_CRASH_KEY_STRING64("tracing", "AppId", base::WideToASCII(app_id));
    // The result of opening the AppId key; should be 0.
    SCOPED_CRASH_KEY_NUMBER("tracing", "app_id_key_result", app_id_key_result);
    // The result of reading LocalService from the AppId key; should be 0.
    SCOPED_CRASH_KEY_NUMBER("tracing", "local_service_value_result",
                            local_service_value_result);
    // The LocalService value. For dev channel, this should be
    // "GoogleChromeDevTracingService".
    SCOPED_CRASH_KEY_STRING64("tracing", "LocalService",
                              base::WideToASCII(local_service));
    // The name of the service as registered with the SCM. This should match the
    // LocalService value.
    SCOPED_CRASH_KEY_STRING64("tracing", "service_name",
                              base::WideToASCII(service_name));
    // The result of opening the service's registry key; should be 0.
    SCOPED_CRASH_KEY_NUMBER("tracing", "service_key_result",
                            service_key_result);
    // The result of reading the service's ImagePath; should be 0.
    SCOPED_CRASH_KEY_NUMBER("tracing", "image_path_value_result",
                            image_path_value_result);
    // The ImagePath value; should be the command line for the service.
    SCOPED_CRASH_KEY_STRING256("tracing", "ImagePath",
                               base::WideToASCII(image_path));
    // The result of querying file attributes of the executable; should be 0.
    SCOPED_CRASH_KEY_NUMBER("tracing", "get_attributes_result",
                            get_attributes_result);

    base::debug::Alias(&info);
    base::debug::DumpWithoutCrashing();  // https://crbug.com/392441188.
    return true;
  }();
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

  RecordLaunchResult(LaunchStage::kCoCreateInstance, hresult);
  if (FAILED(hresult)) {
    // Creation is expected to fail with REGDB_E_CLASSNOTREG for per-user
    // installs.
    if (hresult != REGDB_E_CLASSNOTREG || !InstallUtil::IsPerUserInstall()) {
      ReportFirstStartFailure(clsid_, hresult);
    }
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
