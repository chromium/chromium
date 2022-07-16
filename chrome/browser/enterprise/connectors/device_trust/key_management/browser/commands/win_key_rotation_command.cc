// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/win_key_rotation_command.h"

#include <comutil.h>
#include <objbase.h>
#include <oleauto.h>
#include <unknwn.h>
#include <windows.h>
#include <winerror.h>
#include <wrl/client.h>

#include "base/base64.h"
#include "base/win/scoped_bstr.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/util_constants.h"
#include "google_update/google_update_idl.h"

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

}  // namespace

WinKeyRotationCommand::WinKeyRotationCommand() = default;

WinKeyRotationCommand::~WinKeyRotationCommand() = default;

bool WinKeyRotationCommand::Trigger(const KeyRotationCommand::Params& params) {
  Microsoft::WRL::ComPtr<IGoogleUpdate3Web> google_update;
  HRESULT hr = ::CoCreateInstance(CLSID_GoogleUpdate3WebServiceClass, nullptr,
                                  CLSCTX_ALL, IID_PPV_ARGS(&google_update));
  if (FAILED(hr))
    return false;

  ConfigureProxyBlanket(google_update.Get());
  Microsoft::WRL::ComPtr<IDispatch> dispatch;
  hr = google_update->createAppBundleWeb(&dispatch);
  if (FAILED(hr))
    return false;

  Microsoft::WRL::ComPtr<IAppBundleWeb> app_bundle;
  hr = dispatch.As(&app_bundle);
  if (FAILED(hr))
    return false;

  dispatch.Reset();
  ConfigureProxyBlanket(app_bundle.Get());
  app_bundle->initialize();
  const wchar_t* app_guid = install_static::GetAppGuid();
  hr = app_bundle->createInstalledApp(base::win::ScopedBstr(app_guid).Get());
  if (FAILED(hr))
    return false;

  hr = app_bundle->get_appWeb(0, &dispatch);
  if (FAILED(hr))
    return false;

  Microsoft::WRL::ComPtr<IAppWeb> app;
  hr = dispatch.As(&app);
  if (FAILED(hr))
    return false;

  dispatch.Reset();
  ConfigureProxyBlanket(app.Get());
  hr = app->get_command(
      base::win::ScopedBstr(installer::kCmdRotateDeviceTrustKey).Get(),
      &dispatch);
  if (FAILED(hr) || !dispatch)
    return false;

  Microsoft::WRL::ComPtr<IAppCommandWeb> app_command;
  hr = dispatch.As(&app_command);
  if (FAILED(hr))
    return false;

  ConfigureProxyBlanket(app_command.Get());
  std::string token_base64;
  base::Base64Encode(params.dm_token, &token_base64);
  VARIANT var;
  VariantInit(&var);
  _variant_t token_var = token_base64.c_str();
  _variant_t dm_server_url_var = params.dm_server_url.c_str();
  _variant_t nonce_var = params.nonce.c_str();
  hr = app_command->execute(token_var, dm_server_url_var, nonce_var, var, var,
                            var, var, var, var);
  if (FAILED(hr))
    return false;

  // TODO(crbug.com/823515): Get the status of the app command execution and
  // return a corresponding value for |success|. For now, assume that the call
  // to setup.exe succeeds.
  return true;
}

}  // namespace enterprise_connectors
