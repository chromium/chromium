// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"

#include <string>
#include <string_view>

#include "base/strings/strcat_internal.h"

namespace base {

std::string StrCat(span<const std::string_view> pieces) {
  return internal::StrCatT(pieces);
}

std::u16string StrCat(span<const std::u16string_view> pieces) {
  return internal::StrCatT(pieces);
}

std::string StrCat(span<const std::string> pieces) {
  return internal::StrCatT(pieces);
}

std::u16string StrCat(span<const std::u16string> pieces) {
  return internal::StrCatT(pieces);
}

std::string StrCat(std::initializer_list<std::string_view> pieces) {
  return internal::StrCatT(span(pieces));
}

std::u16string StrCat(std::initializer_list<std::u16string_view> pieces) {
  return internal::StrCatT(span(pieces));
}

void StrAppend(std::string* dest, span<const std::string_view> pieces) {
  internal::StrAppendT(*dest, pieces);
}

void StrAppend(std::u16string* dest, span<const std::u16string_view> pieces) {
  internal::StrAppendT(*dest, pieces);
}

void StrAppend(std::string* dest, span<const std::string> pieces) {
  internal::StrAppendT(*dest, pieces);
}

void StrAppend(std::u16string* dest, span<const std::u16string> pieces) {
  internal::StrAppendT(*dest, pieces);
}

void StrAppend(std::string* dest,
               std::initializer_list<std::string_view> pieces) {
  internal::StrAppendT(*dest, span(pieces));
}

void StrAppend(std::u16string* dest,
               std::initializer_list<std::u16string_view> pieces) {
  internal::StrAppendT(*dest, span(pieces));
}

}  // namespace base
