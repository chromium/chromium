// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/latin1_string_conversions.h"

namespace base {

std::u16string Latin1OrUTF16ToUTF16(size_t length,
                                    const Latin1Char* latin1,
                                    const char16_t* utf16) {
  if (!length)
    return std::u16string();
  if (latin1)
    return std::u16string(latin1, latin1 + length);
  return std::u16string(utf16, utf16 + length);
}

}  // namespace base
