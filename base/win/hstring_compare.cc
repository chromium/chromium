// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/hstring_compare.h"

#include <winstring.h>

#include "base/native_library.h"
#include "base/win/windows_version.h"

namespace base {
namespace win {

HRESULT HStringCompare(HSTRING string1, HSTRING string2, INT32* result) {
  using CompareStringFunc = decltype(&::WindowsCompareStringOrdinal);

  static const auto compare_string_func = []() -> CompareStringFunc {
    if (GetVersion() < Version::WIN8)
      return nullptr;

    NativeLibraryLoadError load_error;
    NativeLibrary combase_module =
        PinSystemLibrary(FILE_PATH_LITERAL("combase.dll"), &load_error);
    if (load_error.code)
      return nullptr;

    return reinterpret_cast<CompareStringFunc>(
        GetFunctionPointerFromNativeLibrary(combase_module,
                                            "WindowsCompareStringOrdinal"));
  }();

  if (!compare_string_func)
    return E_FAIL;

  return compare_string_func(string1, string2, result);
}

}  // namespace win
}  // namespace base
