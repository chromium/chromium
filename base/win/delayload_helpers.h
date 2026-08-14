// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_DELAYLOAD_HELPERS_H_
#define BASE_WIN_DELAYLOAD_HELPERS_H_

#include <windows.h>

#include <delayimp.h>

#include "base/base_export.h"
#include "base/strings/cstring_view.h"
#include "base/types/expected.h"

namespace base::win {

// Resolves all delayload imports for `dll_name` rather than doing so when the
// functions are first called. Returns `true` if the attempt succeeded or
// `false` if the module is not a delayloaded dep of the current module (this
// often happens in tests or the component build). Errors incurred during the
// load (e.g., a failure to initialize the module, or unknown exports) result in
// a crash; see `HandleDelayLoadFailureCommon`. This helper is `inline` so that
// the module calling this helper is the one that attempts the import (rather
// than base.dll in the component build), and brings in `<windows.h>`.
//
// See docs for __HrLoadAllImportsForDll() at
// https://learn.microsoft.com/en-us/cpp/build/reference/linker-support-for-delay-loaded-dlls
//
// Note that `dll_name` is case-sensitive including the dll extension and must
// match the name listed in the current module's delayloaded imports section.
inline bool LoadAllImportsForDll(base::cstring_view dll_name) {
  HRESULT hr = ::__HrLoadAllImportsForDll(dll_name.c_str());

  if (hr == HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND)) {
    // __HrLoadAllImportsForDll returns this exact value (FACILITY_WIN32) if the
    // module is not found in the calling module's list of delay imports. This
    // may be the case in the component build or in tests, where the module may
    // be delayloaded by some module other than chrome.dll or the test binary.
    return false;
  }
  return SUCCEEDED(hr);
}

namespace delayload_internal {

// Scoped object that suppresses delay-load failure crashing on the current
// thread, allowing delay-load errors to raise SEH exceptions that can be caught
// by `LoadAllImportsForDllUnchecked`.
class BASE_EXPORT ScopedSuppressDelayLoadFailure {
 public:
  ScopedSuppressDelayLoadFailure();
  ~ScopedSuppressDelayLoadFailure();

  ScopedSuppressDelayLoadFailure(const ScopedSuppressDelayLoadFailure&) =
      delete;
  ScopedSuppressDelayLoadFailure& operator=(
      const ScopedSuppressDelayLoadFailure&) = delete;

 private:
  const bool previous_value_;
};

}  // namespace delayload_internal

// As `LoadAllImportsForDll`, but returns an HRESULT on error rather than
// crashing the process. This can be used to speculatively load and resolve
// imports for a module so that errors can be gracefully handled; for example,
// by providing a downgraded experience.
inline base::expected<bool, HRESULT> LoadAllImportsForDllUnchecked(
    base::cstring_view dll_name) {
  delayload_internal::ScopedSuppressDelayLoadFailure suppress;
  HRESULT hr = E_FAIL;
  __try {
    hr = ::__HrLoadAllImportsForDll(dll_name.c_str());

    if (hr == HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND)) {
      // __HrLoadAllImportsForDll returns this exact value (FACILITY_WIN32) if
      // the module is not found in the calling module's list of delay imports.
      // This may be the case in the component build or in tests, where the
      // module may be delayloaded by some module other than chrome.dll or the
      // test binary.
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

// Returns true if the current thread is executing within
// `LoadAllImportsForDllUnchecked`. This should be used by a module-specific
// delayload failure hook to determine whether or not failure processing (e.g.,
// process termination) for a specific failure should be suppressed.
BASE_EXPORT bool IsDelayLoadFailureSuppressed();

}  // namespace base::win

#endif  // BASE_WIN_DELAYLOAD_HELPERS_H_
