// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/to_string.h"

#include <string>
#include <string_view>

#include "base/strings/utf_string_conversions.h"

namespace base {

std::string ToString(std::string_view sv) {
  return std::string(sv);
}

std::string ToString(std::u16string_view sv) {
  return UTF16ToUTF8(sv);
}

std::string ToString(std::wstring_view sv) {
  return WideToUTF8(sv);
}

}  // namespace base
