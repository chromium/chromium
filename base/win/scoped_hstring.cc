// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/scoped_hstring.h"

#include <winstring.h>

#include "base/check.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/memory.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"

namespace base {

namespace {

static bool g_load_succeeded = false;

FARPROC LoadComBaseFunction(const char* function_name) {
  static HMODULE const handle =
      ::LoadLibraryEx(L"combase.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
  return handle ? ::GetProcAddress(handle, function_name) : nullptr;
}

decltype(&::WindowsCreateString) GetWindowsCreateString() {
  static decltype(&::WindowsCreateString) const function =
      reinterpret_cast<decltype(&::WindowsCreateString)>(
          LoadComBaseFunction("WindowsCreateString"));
  return function;
}

decltype(&::WindowsDeleteString) GetWindowsDeleteString() {
  static decltype(&::WindowsDeleteString) const function =
      reinterpret_cast<decltype(&::WindowsDeleteString)>(
          LoadComBaseFunction("WindowsDeleteString"));
  return function;
}

decltype(&::WindowsGetStringRawBuffer) GetWindowsGetStringRawBuffer() {
  static decltype(&::WindowsGetStringRawBuffer) const function =
      reinterpret_cast<decltype(&::WindowsGetStringRawBuffer)>(
          LoadComBaseFunction("WindowsGetStringRawBuffer"));
  return function;
}

HRESULT WindowsCreateString(const wchar_t* src,
                            uint32_t len,
                            HSTRING* out_hstr) {
  decltype(&::WindowsCreateString) create_string_func =
      GetWindowsCreateString();
  if (!create_string_func)
    return E_FAIL;
  return create_string_func(src, len, out_hstr);
}

HRESULT WindowsDeleteString(HSTRING hstr) {
  decltype(&::WindowsDeleteString) delete_string_func =
      GetWindowsDeleteString();
  if (!delete_string_func)
    return E_FAIL;
  return delete_string_func(hstr);
}

const wchar_t* WindowsGetStringRawBuffer(HSTRING hstr, uint32_t* out_len) {
  decltype(&::WindowsGetStringRawBuffer) get_string_raw_buffer_func =
      GetWindowsGetStringRawBuffer();
  if (!get_string_raw_buffer_func) {
    *out_len = 0;
    return nullptr;
  }
  return get_string_raw_buffer_func(hstr, out_len);
}

}  // namespace

namespace internal {

// static
void ScopedHStringTraits::Free(HSTRING hstr) {
  base::WindowsDeleteString(hstr);
}

}  // namespace internal

namespace win {

ScopedHString::ScopedHString(HSTRING hstr) : ScopedGeneric(hstr) {
  DCHECK(g_load_succeeded);
}

// static
ScopedHString ScopedHString::Create(WStringPiece str) {
  DCHECK(g_load_succeeded);
  HSTRING hstr;
  HRESULT hr = base::WindowsCreateString(
      str.data(), checked_cast<UINT32>(str.length()), &hstr);
  if (SUCCEEDED(hr))
    return ScopedHString(hstr);

  if (hr == E_OUTOFMEMORY) {
    // This size is an approximation. The actual size likely includes
    // sizeof(HSTRING_HEADER) as well.
    base::TerminateBecauseOutOfMemory((str.length() + 1) * sizeof(wchar_t));
  }

  // This should not happen at runtime. Otherwise we could silently pass nullptr
  // or an empty string to downstream code.
  NOTREACHED() << "Failed to create HSTRING: " << std::hex << hr;
  return ScopedHString(nullptr);
}

// static
ScopedHString ScopedHString::Create(StringPiece str) {
  return Create(UTF8ToWide(str));
}

// static
bool ScopedHString::ResolveCoreWinRTStringDelayload() {
  // TODO(finnur): Add AssertIOAllowed once crbug.com/770193 is fixed.

  static const bool load_succeeded = []() {
    bool success = GetWindowsCreateString() && GetWindowsDeleteString() &&
                   GetWindowsGetStringRawBuffer();
    g_load_succeeded = success;
    return success;
  }();
  return load_succeeded;
}

WStringPiece ScopedHString::Get() const {
  UINT32 length = 0;
  const wchar_t* buffer = base::WindowsGetStringRawBuffer(get(), &length);
  return WStringPiece(buffer, length);
}

std::string ScopedHString::GetAsUTF8() const {
  return WideToUTF8(Get());
}

}  // namespace win
}  // namespace base
