// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"

#include <string>

#include "base/strings/strcat_internal.h"

namespace base {

std::string StrCat(span<const StringPiece> pieces) {
  return internal::StrCatT(pieces);
}

std::u16string StrCat(span<const StringPiece16> pieces) {
  return internal::StrCatT(pieces);
}

std::string StrCat(span<const std::string> pieces) {
  return internal::StrCatT(pieces);
}

std::u16string StrCat(span<const std::u16string> pieces) {
  return internal::StrCatT(pieces);
}

void StrAppend(std::string* dest, span<const StringPiece> pieces) {
  internal::StrAppendT(*dest, pieces);
}

void StrAppend(std::u16string* dest, span<const StringPiece16> pieces) {
  internal::StrAppendT(*dest, pieces);
}

void StrAppend(std::string* dest, span<const std::string> pieces) {
  internal::StrAppendT(*dest, pieces);
}

void StrAppend(std::u16string* dest, span<const std::u16string> pieces) {
  internal::StrAppendT(*dest, pieces);
}

}  // namespace base
