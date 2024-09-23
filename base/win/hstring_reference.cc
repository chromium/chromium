// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/hstring_reference.h"

#include <windows.h>

#include <wchar.h>
#include <winstring.h>

#include "base/check_op.h"
#include "base/numerics/safe_conversions.h"

namespace base::win {

HStringReference::HStringReference(const wchar_t* str) {
  // String must be null terminated for WindowsCreateStringReference.
  // nullptr str is OK so long as the length is 0.
  size_t length = str ? wcslen(str) : 0;
  const HRESULT hr = ::WindowsCreateStringReference(
      str, checked_cast<UINT32>(length), &hstring_header_, &hstring_);
  DCHECK_EQ(hr, S_OK);
}

}  // namespace base::win
