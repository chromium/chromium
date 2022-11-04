// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>
#include <tuple>

#include <fuzzer/FuzzedDataProvider.h>

#include "base/strings/pattern.h"
#include "base/strings/utf_string_conversions.h"

namespace {

// Prevent huge inputs from hitting time limits.
constexpr size_t kMaxLength = 1000;

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);
  std::string string = provider.ConsumeRandomLengthString(kMaxLength);
  std::string pattern = provider.ConsumeRandomLengthString(kMaxLength);

  std::ignore = base::MatchPattern(string, pattern);
  // Test the wide-string version as well. Note that the Unicode conversion
  // function skips errors (returning the best conversion possible), which is
  // good enough for the fuzzer.
  std::ignore =
      base::MatchPattern(base::UTF8ToUTF16(string), base::UTF8ToUTF16(pattern));

  return 0;
}
