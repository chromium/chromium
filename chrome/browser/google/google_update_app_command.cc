// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_update_app_command.h"

#include "base/win/scoped_bstr.h"
#include "chrome/install_static/install_util.h"

void ConfigureProxyBlanket(IUnknown* interface_pointer) {
  ::CoSetProxyBlanket(
      interface_pointer, RPC_C_AUTHN_DEFAULT, RPC_C_AUTHZ_DEFAULT,
      COLE_DEFAULT_PRINCIPAL, RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
      RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_DYNAMIC_CLOAKING);
}

base::expected<Microsoft::WRL::ComPtr<IAppCommandWeb>, HRESULT>
GetUpdaterAppCommand(const std::wstring& command_name) {
  Microsoft::WRL::ComPtr<IUnknown> server;
  HRESULT hr = ::CoCreateInstance(CLSID_GoogleUpdate3WebSystemClass, nullptr,
                                  CLSCTX_ALL, IID_PPV_ARGS(&server));
  if (FAILED(hr)) {
    return base::unexpected(hr);
  }

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
      return base::unexpected(hr);
    }
  }

  Microsoft::WRL::ComPtr<IDispatch> dispatch;
  hr = google_update->createAppBundleWeb(&dispatch);
  if (FAILED(hr)) {
    return base::unexpected(hr);
  }

  // Chrome queries for the SxS IIDs first, with a fallback to the legacy IID.
  // Without this change, marshaling can load the typelib from the wrong hive
  // (HKCU instead of HKLM, or vice-versa).
  Microsoft::WRL::ComPtr<IAppBundleWeb> app_bundle;
  hr = dispatch.CopyTo(__uuidof(IAppBundleWebSystem),
                       IID_PPV_ARGS_Helper(&app_bundle));
  if (FAILED(hr)) {
    hr = dispatch.As(&app_bundle);
    if (FAILED(hr)) {
      return base::unexpected(hr);
    }
  }

  dispatch.Reset();
  ConfigureProxyBlanket(app_bundle.Get());
  app_bundle->initialize();
  const wchar_t* app_guid = install_static::GetAppGuid();
  hr = app_bundle->createInstalledApp(base::win::ScopedBstr(app_guid).Get());
  if (FAILED(hr)) {
    return base::unexpected(hr);
  }

  hr = app_bundle->get_appWeb(0, &dispatch);
  if (FAILED(hr)) {
    return base::unexpected(hr);
  }

  Microsoft::WRL::ComPtr<IAppWeb> app;
  hr = dispatch.CopyTo(__uuidof(IAppWebSystem), IID_PPV_ARGS_Helper(&app));
  if (FAILED(hr)) {
    hr = dispatch.As(&app);
    if (FAILED(hr)) {
      return base::unexpected(hr);
    }
  }

  dispatch.Reset();
  ConfigureProxyBlanket(app.Get());

  hr = app->get_command(base::win::ScopedBstr(command_name).Get(), &dispatch);
  if (FAILED(hr) || !dispatch) {
    return base::unexpected(hr);
  }

  Microsoft::WRL::ComPtr<IAppCommandWeb> app_command;
  hr = dispatch.CopyTo(__uuidof(IAppCommandWebSystem),
                       IID_PPV_ARGS_Helper(&app_command));
  if (FAILED(hr)) {
    hr = dispatch.As(&app_command);
    if (FAILED(hr)) {
      return base::unexpected(hr);
    }
  }

  ConfigureProxyBlanket(app_command.Get());
  return base::ok(app_command);
}
