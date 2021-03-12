// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_STRINGS_STRING16_H_
#define BASE_STRINGS_STRING16_H_

// WHAT:
// Type aliases for string and character types supporting UTF-16 data. Prior to
// C++11 there was no standard library solution for this, which is why wstring
// was used where possible (i.e. where wchar_t holds UTF-16 encoded data).
//
// In C++11 we gained std::u16string, which is a cross-platform solution for
// UTF-16 strings. This is now the string16 type where ever wchar_t does not
// hold UTF16 data (i.e. commonly non-Windows platforms). Eventually this should
// be used everywhere, at which point this type alias and this file should be
// removed. https://crbug.com/911896 tracks the migration effort.

#include <string>

namespace base {
using string16 = std::u16string;
}  // namespace base

#endif  // BASE_STRINGS_STRING16_H_
