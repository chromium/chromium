// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/scoped_hstring.h"

#include <winstring.h>

#include <ostream>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/memory.h"

#include "base/strings/utf_string_conversions.h"

namespace base {

namespace internal {

// static
void ScopedHStringTraits::Free(HSTRING hstr) {
  ::WindowsDeleteString(hstr);
}

}  // namespace internal

namespace win {

ScopedHString::ScopedHString(HSTRING hstr) : ScopedGeneric(hstr) {}

// static
ScopedHString ScopedHString::Create(std::wstring_view str) {
  HSTRING hstr;
  HRESULT hr = ::WindowsCreateString(str.data(),
                                     checked_cast<UINT32>(str.length()), &hstr);
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
}

// static
ScopedHString ScopedHString::Create(std::string_view str) {
  return Create(UTF8ToWide(str));
}

// static
std::wstring_view ScopedHString::Get() const {
  UINT32 length = 0;
  const wchar_t* buffer = ::WindowsGetStringRawBuffer(get(), &length);
  return std::wstring_view(buffer, length);
}

std::string ScopedHString::GetAsUTF8() const {
  return WideToUTF8(Get());
}

}  // namespace win
}  // namespace base
