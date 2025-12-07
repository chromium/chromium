// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_DELAYLOAD_HELPERS_H_
#define BASE_WIN_DELAYLOAD_HELPERS_H_

#include <windows.h>

#include <delayimp.h>

#include "base/strings/cstring_view.h"
#include "base/types/expected.h"

namespace base::win {

// Resolves all delayload imports for `module` rather than doing so when the
// functions are first called. Returns `bool:true` if the attempt succeeded,
// `bool:false` if the module is not a delayloaded dep of the current module
// (this often happens in tests or the component build), or an `HRESULT` error
// otherwise. This helper is `inline` so that the module calling this helper
// is the one that attempts the import (rather than base.dll in the component
// build), and brings in `<windows.h>`.
//
// See docs for __HrLoadAllImportsForDll() at
// https://learn.microsoft.com/en-us/cpp/build/reference/linker-support-for-delay-loaded-dlls
//
// Note that `dll_name` is case-sensitive including the dll extension and must
// match the name listed in the current module's delayloaded imports section.
inline base::expected<bool, HRESULT> LoadAllImportsForDll(
    base::cstring_view dll_name) {
  HRESULT hr = E_FAIL;
  __try {
    hr = ::__HrLoadAllImportsForDll(dll_name.c_str());

    if (hr == HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND)) {
      // __HrLoadAllImportsForDll returns this exact value (FACILITY_WIN32)
      // if the module is not found in the calling module's list of delay
      // imports. This may be the case in the component build or in tests,
      // where the module may be delayloaded by some module other than
      // chrome.dll or the test binary.
      return base::ok(false);
    }
  } __except (HRESULT_FACILITY(::GetExceptionCode()) == FACILITY_VISUALCPP
                  ? EXCEPTION_EXECUTE_HANDLER
                  : EXCEPTION_CONTINUE_SEARCH) {
    // Resolution of all imports failed; possibly because the module failed to
    // load or because one or more imports was not found. Note that the filter
    // expression above matches exceptions where the code is an HRESULT with the
    // facility bits set to FACILITY_VISUALCPP, so the following cast is safe.
    hr = static_cast<HRESULT>(::GetExceptionCode());
  }
  if (FAILED(hr)) {
    return base::unexpected(hr);
  }
  return base::ok(true);
}

}  // namespace base::win

#endif  // BASE_WIN_DELAYLOAD_HELPERS_H_
