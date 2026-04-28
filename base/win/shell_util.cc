// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/shell_util.h"

#include <objbase.h>

#include <windows.h>

#include <shlobj.h>
#include <wrl/client.h>

#include <string>
#include <utility>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/win/com_init_util.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_variant.h"

namespace base::win {

HRESULT RunShellExecuteViaExplorer(const std::wstring& path,
                                   const std::wstring& parameters,
                                   const ShellExecuteOptions& options) {
  base::win::AssertComInitialized();

  HRESULT hr;

  Microsoft::WRL::ComPtr<IDispatch> window_dispatch;
  {
    Microsoft::WRL::ComPtr<IShellWindows> shell;
    hr = ::CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_LOCAL_SERVER,
                            IID_PPV_ARGS(&shell));
    if (FAILED(hr)) {
      return hr;
    }

    LONG hwnd = 0;
    hr = shell->FindWindowSW(ScopedVariant(CSIDL_DESKTOP).AsInput(),
                             ScopedVariant().AsInput(), SWC_DESKTOP, &hwnd,
                             SWFO_NEEDDISPATCH, &window_dispatch);
    if (hr == S_FALSE || FAILED(hr)) {
      return hr == S_FALSE ? E_FAIL : hr;
    }

    // Allow the Explorer process to take the foreground. This ensures that the
    // launched application or any Shell UI (like "Open With" or "Mark of the
    // Web" dialogs) appears in front of the current window.
    DWORD explorer_pid = 0;
    GetWindowThreadProcessId(reinterpret_cast<HWND>(hwnd), &explorer_pid);
    if (explorer_pid) {
      ::AllowSetForegroundWindow(explorer_pid);
    }
  }

  Microsoft::WRL::ComPtr<IDispatch> background_dispatch;
  {
    Microsoft::WRL::ComPtr<IServiceProvider> service;
    hr = window_dispatch.As(&service);
    if (FAILED(hr)) {
      return hr;
    }

    Microsoft::WRL::ComPtr<IShellBrowser> browser;
    hr = service->QueryService(SID_STopLevelBrowser, IID_PPV_ARGS(&browser));
    if (FAILED(hr)) {
      return hr;
    }

    Microsoft::WRL::ComPtr<IShellView> view;
    hr = browser->QueryActiveShellView(&view);
    if (FAILED(hr)) {
      return hr;
    }

    hr = view->GetItemObject(SVGIO_BACKGROUND,
                             IID_PPV_ARGS(&background_dispatch));
    if (FAILED(hr)) {
      return hr;
    }
  }

  Microsoft::WRL::ComPtr<IShellDispatch2> shell_dispatch;
  {
    Microsoft::WRL::ComPtr<IShellFolderViewDual> folder;
    hr = background_dispatch.As(&folder);
    if (FAILED(hr)) {
      return hr;
    }

    Microsoft::WRL::ComPtr<IDispatch> application_dispatch;
    hr = folder->get_Application(&application_dispatch);
    if (FAILED(hr)) {
      return hr;
    }

    hr = application_dispatch.As(&shell_dispatch);
    if (FAILED(hr)) {
      return hr;
    }
  }

  std::wstring current_directory = options.current_directory;
  if (current_directory.empty()) {
    FilePath current_dir;
    if (PathService::Get(DIR_CURRENT, &current_dir)) {
      current_directory = current_dir.value();
    }
  }

  return shell_dispatch->ShellExecute(
      ScopedBstr(path.c_str()).Get(), ScopedVariant(parameters.c_str()),
      ScopedVariant(current_directory.data(),
                    static_cast<UINT>(current_directory.length())),
      options.verb.empty() ? ScopedVariant::kEmptyVariant
                           : ScopedVariant(options.verb.c_str()),
      ScopedVariant(options.start_hidden ? SW_HIDE : SW_SHOWDEFAULT));
}

}  // namespace base::win
