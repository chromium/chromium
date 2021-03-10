// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_UCHAR_H_
#define BASE_I18N_UCHAR_H_

#include "base/strings/string16.h"
#include "third_party/icu/source/common/unicode/utypes.h"

// This file contains functions to convert between C-strings of character types
// `UChar` and `base::char16`. This allows to change the underlying types
// independently, simplifying the migration of both types to char16_t.
// Naming and functionality of these functions is inspired by ICU's toUCharPtr.
//
// TODO(crbug.com/911896): Remove these functions once `base::char16` and
// `UChar` are char16_t on all platforms.
namespace base {
namespace i18n {

static_assert(sizeof(UChar) == sizeof(char16_t),
              "Error: UChar and char16_t are not of the same size.");

inline const UChar* ToUCharPtr(const char16_t* str) {
  return reinterpret_cast<const UChar*>(str);
}

inline UChar* ToUCharPtr(char16_t* str) {
  return reinterpret_cast<UChar*>(str);
}

inline const char16_t* ToChar16Ptr(const UChar* str) {
  return reinterpret_cast<const char16_t*>(str);
}

inline char16_t* ToChar16Ptr(UChar* str) {
  return reinterpret_cast<char16_t*>(str);
}

}  // namespace i18n
}  // namespace base

#endif  // BASE_I18N_UCHAR_H_
