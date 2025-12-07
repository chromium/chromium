// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/strings/string_split.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "build/build_config.h"

namespace base {

namespace {

std::string MakeString() { return std::string(); }
std::u16string MakeString16() { return std::u16string(); }
#if BUILDFLAG(IS_WIN)
std::wstring MakeWideString() { return std::wstring(); }
#endif  // BUILDFLAG(IS_WIN)

void DanglingSplitOnce() {
  [[maybe_unused]] auto r1 = SplitStringOnce(MakeString(), '.');  // expected-error {{object backing the pointer will be destroyed at the end of the full-expression}}
  [[maybe_unused]] auto r2 = SplitStringOnce(MakeString(), ".,");  // expected-error {{object backing the pointer will be destroyed at the end of the full-expression}}

  [[maybe_unused]] auto r3 = RSplitStringOnce(MakeString(), '.');  // expected-error {{object backing the pointer will be destroyed at the end of the full-expression}}
  [[maybe_unused]] auto r4 = RSplitStringOnce(MakeString(), "&;");  // expected-error {{object backing the pointer will be destroyed at the end of the full-expression}}
}

void DanglingSplitStringPiece() {
  [[maybe_unused]] auto v1 = SplitStringPiece(
      MakeString(), "&;", TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY);  // expected-error {{object backing the pointer will be destroyed at the end of the full-expression}}
  [[maybe_unused]] auto v2 = SplitStringPiece(
      MakeString16(), u"&;", TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY);  // expected-error {{object backing the pointer will be destroyed at the end of the full-expression}}
#if BUILDFLAG(IS_WIN)
  [[maybe_unused]] auto v3 = SplitStringPiece(
      MakeWideString(), L"&;", TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY);  // expected-error {{object backing the pointer will be destroyed at the end of the full-expression}}
#endif  // BUILDFLAG(IS_WIN)
}

void DanglinSplitStringPieceUsingSubstr() {
  [[maybe_unused]] auto v1 = SplitStringPieceUsingSubstr(
      MakeString(), " and ", TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY);  // expected-error {{object backing the pointer will be destroyed at the end of the full-expression}}
  [[maybe_unused]] auto v2 = SplitStringPieceUsingSubstr(
      MakeString16(), u" and ", TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY);  // expected-error {{object backing the pointer will be destroyed at the end of the full-expression}}
#if BUILDFLAG(IS_WIN)
  [[maybe_unused]] auto v3 = SplitStringPieceUsingSubstr(
      MakeWideString(), L" and ", TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY);  // expected-error {{object backing the pointer will be destroyed at the end of the full-expression}}
#endif  // BUILDFLAG(IS_WIN)
}

}

}  // namespace base
