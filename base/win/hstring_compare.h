// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_HSTRING_COMPARE_H_
#define BASE_WIN_HSTRING_COMPARE_H_

#include <hstring.h>

#include "base/base_export.h"

namespace base {
namespace win {

// HStringCompare provides a delayloaded version of WindowsCompareStringOrdinal
// function, which compares HSTRING values.
//
// Note that it requires certain functions that are only available on Windows 8
// and later, and that these functions need to be delayloaded to avoid breaking
// Chrome on Windows 7.
BASE_EXPORT HRESULT HStringCompare(HSTRING string1,
                                   HSTRING string2,
                                   INT32* result);

}  // namespace win
}  // namespace base

#endif  // BASE_WIN_HSTRING_COMPARE_H_
