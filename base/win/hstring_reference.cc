// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/hstring_reference.h"

#include <windows.h>

#include <winstring.h>

#include "base/check_op.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/char_traits.h"

namespace base {
namespace {

bool g_winrt_string_loaded = false;

decltype(&::WindowsCreateStringReference) GetWindowsCreateStringReference() {
  static auto const create_string_reference_func =
      []() -> decltype(&::WindowsCreateStringReference) {
    const HMODULE handle =
        ::LoadLibraryEx(L"combase.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (handle) {
      return reinterpret_cast<decltype(&::WindowsCreateStringReference)>(
          ::GetProcAddress(handle, "WindowsCreateStringReference"));
    }
    return nullptr;
  }();
  return create_string_reference_func;
}

}  // namespace

namespace win {

// static
bool HStringReference::ResolveCoreWinRTStringDelayload() {
  g_winrt_string_loaded = GetWindowsCreateStringReference() != nullptr;
  return g_winrt_string_loaded;
}

HStringReference::HStringReference(const wchar_t* str, size_t length) {
  DCHECK(g_winrt_string_loaded);
  // String must be null terminated for WindowsCreateStringReference.
  // nullptr str is OK so long as the length is 0.
  DCHECK(str ? str[length] == L'\0' : length == 0);
  // If you nullptr crash here, you've failed to call
  // ResolveCoreWinRTStringDelayLoad and check its return value.
  const HRESULT hr = GetWindowsCreateStringReference()(
      str, checked_cast<UINT32>(length), &hstring_header_, &hstring_);
  // All failure modes of WindowsCreateStringReference are handled gracefully
  // but this class.
  DCHECK_EQ(hr, S_OK);
}

HStringReference::HStringReference(const wchar_t* str)
    : HStringReference(str, str ? CharTraits<wchar_t>::length(str) : 0) {}

}  // namespace win
}  // namespace base
