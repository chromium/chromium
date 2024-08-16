// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/win_key_rotation_command.h"

#include <comutil.h>
#include <winerror.h>
#include <wrl/client.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/syslog_logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/win/scoped_bstr.h"
#include "base/win/windows_types.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/device_trust_constants.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/metrics_utils.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/updater/app/server/win/updater_legacy_idl.h"

namespace enterprise_connectors {

namespace {

// Explicitly allow impersonating the client since some COM code
// elsewhere in the browser process may have previously used
// CoInitializeSecurity to set the impersonation level to something other than
// the default. Ignore errors since an attempt to use Google Update may succeed
// regardless.
void ConfigureProxyBlanket(IUnknown* interface_pointer) {
  ::CoSetProxyBlanket(
      interface_pointer, RPC_C_AUTHN_DEFAULT, RPC_C_AUTHZ_DEFAULT,
      COLE_DEFAULT_PRINCIPAL, RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
      RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_DYNAMIC_CLOAKING);
}

// The maximum number of string that can appear in `args` when calling
// RunGoogleUpdateElevatedCommand().
constexpr int kMaxCommandArgs = 9;

// TODO(rogerta): Should really move this function to a common place where it
// can be called by any code that needs to run an elevated service.  Right now
// this code is duped in two places including this one.
HRESULT RunGoogleUpdateElevatedCommand(const wchar_t* command,
                                       const std::vector<std::string>& args,
                                       std::optional<DWORD>* return_code) {
  DCHECK(return_code);
  if (args.size() > kMaxCommandArgs)
    return E_INVALIDARG;

  Microsoft::WRL::ComPtr<IUnknown> server;
  HRESULT hr = ::CoCreateInstance(CLSID_GoogleUpdate3WebSystemClass, nullptr,
                                  CLSCTX_ALL, IID_PPV_ARGS(&server));
  if (FAILED(hr))
    return hr;

  ConfigureProxyBlanket(server.Get());

  // Chrome queries for the SxS IIDs first, with a fallback to the legacy IID.
  // Without this change, marshaling can load the typelib from the wrong hive
  // (HKCU instead of HKLM, or vice-versa).
  Microsoft::WRL::ComPtr<IGoogleUpdate3Web> google_update;
  hr = server.CopyTo(__uuidof(IGoogleUpdate3WebSystem),
                     IID_PPV_ARGS_Helper(&google_update));
  if (FAILED(hr)) {
    hr = server.As(&google_update);
    if (FAILED(hr)) {
      return hr;
    }
  }

  Microsoft::WRL::ComPtr<IDispatch> dispatch;
  hr = google_update->createAppBundleWeb(&dispatch);
  if (FAILED(hr))
    return hr;

  // Chrome queries for the SxS IIDs first, with a fallback to the legacy IID.
  // Without this change, marshaling can load the typelib from the wrong hive
  // (HKCU instead of HKLM, or vice-versa).
  Microsoft::WRL::ComPtr<IAppBundleWeb> app_bundle;
  hr = dispatch.CopyTo(__uuidof(IAppBundleWebSystem),
                       IID_PPV_ARGS_Helper(&app_bundle));
  if (FAILED(hr)) {
    hr = dispatch.As(&app_bundle);
    if (FAILED(hr)) {
      return hr;
    }
  }

  dispatch.Reset();
  ConfigureProxyBlanket(app_bundle.Get());
  app_bundle->initialize();
  const wchar_t* app_guid = install_static::GetAppGuid();
  hr = app_bundle->createInstalledApp(base::win::ScopedBstr(app_guid).Get());
  if (FAILED(hr))
    return hr;

  hr = app_bundle->get_appWeb(0, &dispatch);
  if (FAILED(hr))
    return hr;

  Microsoft::WRL::ComPtr<IAppWeb> app;
  hr = dispatch.CopyTo(__uuidof(IAppWebSystem), IID_PPV_ARGS_Helper(&app));
  if (FAILED(hr)) {
    hr = dispatch.As(&app);
    if (FAILED(hr)) {
      return hr;
    }
  }

  dispatch.Reset();
  ConfigureProxyBlanket(app.Get());

  hr = app->get_command(base::win::ScopedBstr(command).Get(), &dispatch);
  if (FAILED(hr)) {
    return hr;
  }

  Microsoft::WRL::ComPtr<IAppCommandWeb> app_command;
  hr = dispatch.CopyTo(__uuidof(IAppCommandWebSystem),
                       IID_PPV_ARGS_Helper(&app_command));
  if (FAILED(hr)) {
    hr = dispatch.As(&app_command);
    if (FAILED(hr)) {
      return hr;
    }
  }

  ConfigureProxyBlanket(app_command.Get());

  _variant_t vargs[kMaxCommandArgs];
  for (size_t i = 0; i < args.size(); ++i) {
    vargs[i] = args[i].c_str();
  }

  hr = app_command->execute(vargs[0], vargs[1], vargs[2], vargs[3], vargs[4],
                            vargs[5], vargs[6], vargs[7], vargs[8]);
  if (FAILED(hr))
    return hr;

  // If the call requires the return code of the elevated command, poll until
  // we get it.  Waiting for 10 seconds with a polling frenquency of 1 second
  // are pretty arbitrary choices.
  base::Time wait_until = base::Time::Now() + timeouts::kProcessWaitTimeout;
  UINT status = COMMAND_STATUS_INIT;
  while (base::Time::Now() < wait_until) {
    hr = app_command->get_status(&status);
    if (FAILED(hr) || status == COMMAND_STATUS_ERROR ||
        status == COMMAND_STATUS_COMPLETE) {
      break;
    }

    base::PlatformThread::Sleep(base::Seconds(1));
  }

  // If the command completed get the final exit code.  Otherwise if the
  // command did not terminate in error, tell caller it timed out.
  if (SUCCEEDED(hr)) {
    if (status == COMMAND_STATUS_COMPLETE) {
      DWORD exit_code = 0;
      hr = app_command->get_exitCode(&exit_code);
      if (SUCCEEDED(hr)) {
        *return_code = exit_code;
      }
    } else if (status != COMMAND_STATUS_ERROR) {
      hr = E_ABORT;
    }
  }

  return hr;
}

}  // namespace

WinKeyRotationCommand::WinKeyRotationCommand()
    : WinKeyRotationCommand(
          base::BindRepeating(&RunGoogleUpdateElevatedCommand)) {}

WinKeyRotationCommand::WinKeyRotationCommand(
    RunGoogleUpdateElevatedCommandFn run_elevated_command)
    : WinKeyRotationCommand(
          run_elevated_command,
          base::ThreadPool::CreateCOMSTATaskRunner(
              {base::TaskPriority::USER_BLOCKING, base::MayBlock()})) {}

WinKeyRotationCommand::WinKeyRotationCommand(
    RunGoogleUpdateElevatedCommandFn run_elevated_command,
    scoped_refptr<base::SingleThreadTaskRunner> com_thread_runner)
    : com_thread_runner_(com_thread_runner),
      run_elevated_command_(run_elevated_command) {
  DCHECK(run_elevated_command_);
  DCHECK(com_thread_runner_);
}

WinKeyRotationCommand::~WinKeyRotationCommand() = default;

void WinKeyRotationCommand::Trigger(const KeyRotationCommand::Params& params,
                                    Callback callback) {
  DCHECK(!callback.is_null());

  com_thread_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](const KeyRotationCommand::Params& params,
             RunGoogleUpdateElevatedCommandFn run_elevated_command,
             bool waiting_enabled) {
            if (!install_static::IsSystemInstall()) {
              SYSLOG(ERROR) << "Device trust key rotation failed, browser must "
                               "be a system install.";
              LogKeyRotationCommandError(
                  KeyRotationCommandError::kUserInstallation);
              return KeyRotationCommand::Status::FAILED_INVALID_INSTALLATION;
            }

            std::string token_base64 = base::Base64Encode(params.dm_token);
            std::string nonce_base64 = base::Base64Encode(params.nonce);

            std::optional<DWORD> return_code;

            // Omaha does not support concurrent elevated commands.  If this
            // fails for that reason, wait a little and try again.  Retry count
            // and sleep time are pretty arbitrary choices.
            HRESULT hr = S_OK;
            for (int i = 0; i < 10; ++i) {
              hr = run_elevated_command.Run(
                  installer::kCmdRotateDeviceTrustKey,
                  {token_base64, params.dm_server_url, nonce_base64},
                  &return_code);
              if (hr != GOOPDATE_E_APP_USING_EXTERNAL_UPDATER)
                break;

              if (waiting_enabled)
                base::PlatformThread::Sleep(base::Seconds(1));
            }

            auto status = KeyRotationCommand::Status::FAILED;
            if (SUCCEEDED(hr) && return_code) {
              LogKeyRotationExitCode(return_code.value());
              switch (return_code.value()) {
                case installer::ROTATE_DTKEY_SUCCESS:
                  status = KeyRotationCommand::Status::SUCCEEDED;
                  break;
                case installer::ROTATE_DTKEY_FAILED_PERMISSIONS:
                  status =
                      KeyRotationCommand::Status::FAILED_INVALID_PERMISSIONS;
                  break;
                case installer::ROTATE_DTKEY_FAILED_CONFLICT:
                  status = KeyRotationCommand::Status::FAILED_KEY_CONFLICT;
                  break;
                default:
                  // No-op, status is already marked as failed.
                  break;
              }
            } else if (hr == E_ABORT) {
              status = KeyRotationCommand::Status::TIMED_OUT;
              SYSLOG(ERROR) << "Device trust key rotation timed out.";
              LogKeyRotationCommandError(KeyRotationCommandError::kTimeout);
            } else if (hr == GOOPDATE_E_APP_USING_EXTERNAL_UPDATER) {
              SYSLOG(ERROR) << "Device trust key rotation failed due to Google "
                               "Update concurrency.";
              LogKeyRotationCommandError(
                  KeyRotationCommandError::kUpdaterConcurrency);
            } else if (hr == REGDB_E_CLASSNOTREG) {
              status = KeyRotationCommand::Status::FAILED_INVALID_INSTALLATION;
              SYSLOG(ERROR) << "Device trust key rotation failed, updater "
                               "class not registered.";
              LogKeyRotationCommandError(
                  KeyRotationCommandError::kClassNotRegistered);
            } else if (hr == E_NOINTERFACE) {
              status = KeyRotationCommand::Status::FAILED_INVALID_INSTALLATION;
              SYSLOG(ERROR) << "Device trust key rotation failed, updater "
                               "class does not implement interface";
              LogKeyRotationCommandError(KeyRotationCommandError::kNoInterface);
            } else {
              SYSLOG(ERROR)
                  << "Device trust key rotation failed. HRESULT: " << hr;
              LogKeyRotationCommandError(KeyRotationCommandError::kUnknown);
              LogUnexpectedHresult(hr);
            }
            return status;
          },
          params, run_elevated_command_, waiting_enabled_),
      std::move(callback));
}

}  // namespace enterprise_connectors
