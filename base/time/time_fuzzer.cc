// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>
#include <stddef.h>
#include <stdint.h>

#include <string>
#include <tuple>

#include "base/test/icu_test_util.h"
#include "base/time/time.h"

namespace {

void FuzzStringConversions(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);
  std::string str = provider.ConsumeRemainingBytesAsString();
  base::Time dummy;
  std::ignore = base::Time::FromString(str.c_str(), &dummy);
  std::ignore = base::Time::FromUTCString(str.c_str(), &dummy);
}

void FuzzExplodedConversions(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);
  base::Time::Exploded exploded = {
      .year = provider.ConsumeIntegral<int>(),
      .month = provider.ConsumeIntegral<int>(),
      .day_of_week = provider.ConsumeIntegral<int>(),
      .day_of_month = provider.ConsumeIntegral<int>(),
      .hour = provider.ConsumeIntegral<int>(),
      .minute = provider.ConsumeIntegral<int>(),
      .second = provider.ConsumeIntegral<int>(),
      .millisecond = provider.ConsumeIntegral<int>(),
  };

  base::Time dummy;
  std::ignore = base::Time::FromUTCExploded(exploded, &dummy);
  std::ignore = base::Time::FromLocalExploded(exploded, &dummy);
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Fuzz with a constant time zone to make reproduction of bugs easier.
  base::test::ScopedRestoreDefaultTimezone test_tz("America/Los_Angeles");

  FuzzStringConversions(data, size);
  FuzzExplodedConversions(data, size);

  return 0;
}
