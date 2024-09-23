// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/strings/cstring_view.h"

#include <vector>

namespace base {
namespace {

void WontCompileTypeMismatch() {
  cstring_view(u"123");  // expected-error {{no matching conversion}}
  cstring_view(U"123");  // expected-error {{no matching conversion}}
  u16cstring_view("123");  // expected-error {{no matching conversion}}
  u16cstring_view(U"123");  // expected-error {{no matching conversion}}
  u32cstring_view("123");  // expected-error {{no matching conversion}}
  u32cstring_view(u"123");  // expected-error {{no matching conversion}}

#if BUILDFLAG(IS_WIN)
  cstring_view(L"");  // expected-error {{no matching conversion}}
  u16cstring_view(L"");  // expected-error {{no matching conversion}}
  u32cstring_view(L"");  // expected-error {{no matching conversion}}
  wcstring_view("");  // expected-error {{no matching conversion}}
  wcstring_view(u"");  // expected-error {{no matching conversion}}
  wcstring_view(U"");  // expected-error {{no matching conversion}}
#endif
}

void WontCompileNoNulInArray() {
  const char abc_good[] = {'a', 'b', 'c', '\0'};
  auto v1 = cstring_view(abc_good);  // No error, NUL exists.

#if defined(__clang__)
  const char abc_bad[] = {'a', 'b', 'c'};
  const char after = 'd';
  auto v2 = cstring_view(abc_bad);  // expected-error {{no matching conversion}}
#endif
}

void WontCompilePointerInsteadOfArray() {
  const char good[] = "abc";
  const char* bad = good;
  auto v = cstring_view(bad);  // expected-error {{no matching conversion}}
  auto v2 = cstring_view(nullptr);  // expected-error {{no matching conversion}}
}

void WontCompileCompareTypeMismatch() {
  // TODO(crbug.com/330213589): This should be testable with a static_assert on
  // a concept.
  (void)(cstring_view() == u16cstring_view());  // expected-error {{invalid operands to binary expression}}
  (void)(cstring_view() <=> u16cstring_view());  // expected-error {{invalid operands to binary expression}}
}

void WontCompileSwapTypeMismatch() {
  auto a = cstring_view("8");
  auto b = u16cstring_view(u"16");
  a.swap(b);  // expected-error {{cannot bind to a value of unrelated type}}
}

void WontCompileStartsEndWithMismatch() {
  u16cstring_view(u"abc").starts_with("ab");  // expected-error {{no matching member function}}
  u16cstring_view(u"abc").ends_with("ab");  // expected-error {{no matching member function}}
}

void WontCompileDanglingInput() {
  // TODO: construct from string.
  // auto v1 = cstring_view(std::string("abc"));

  // TODO(https://crbug.com/364890560): uncomment once upstream clang change
  // that warns on this is rolled
  // auto v2 = UNSAFE_BUFFERS(cstring_view(
  //   std::vector<char>{'a', 'b', 'c', '\0'}.data(),
  //   3u));

  auto v3 = cstring_view();
  {
    std::vector<char> abc = {'a', 'b', 'c', '\0'};
    v3 = UNSAFE_BUFFERS(cstring_view(
        abc.data(), 3u));  // This should make a lifetime error but doesn't. :(
  }
}

}  // namespace
}  // namespace base
