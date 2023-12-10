// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat_win.h"

#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/strings/strcat_internal.h"

namespace base {

std::wstring StrCat(span<const std::wstring_view> pieces) {
  return internal::StrCatT(pieces);
}

std::wstring StrCat(span<const std::wstring> pieces) {
  return internal::StrCatT(pieces);
}

void StrAppend(std::wstring* dest, span<const std::wstring_view> pieces) {
  internal::StrAppendT(*dest, pieces);
}

void StrAppend(std::wstring* dest, span<const std::wstring> pieces) {
  internal::StrAppendT(*dest, pieces);
}

}  // namespace base
