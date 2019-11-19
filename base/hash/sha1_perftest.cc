// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/hash/sha1.h"

#include <stddef.h>
#include <stdint.h>
#include <algorithm>
#include <string>
#include <vector>

#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"

static void Timing(const size_t len) {
  std::vector<uint8_t> buf(len);
  base::RandBytes(buf.data(), len);

  const int runs = 111;
  std::vector<base::TimeDelta> utime(runs);
  unsigned char digest[base::kSHA1Length];
  memset(digest, 0, base::kSHA1Length);

  double total_test_time = 0.0;
  for (int i = 0; i < runs; ++i) {
    auto start = base::TimeTicks::Now();
    base::SHA1HashBytes(buf.data(), len, digest);
    auto end = base::TimeTicks::Now();
    utime[i] = end - start;
    total_test_time += utime[i].InMicroseconds();
  }

  std::sort(utime.begin(), utime.end());
  const int med = runs / 2;
  const int min = 0;

  // No need for conversions as length is in bytes and time in usecs:
  // MB/s = (len / (bytes/megabytes)) / (usecs / usecs/sec)
  // MB/s = (len / 1,000,000)/(usecs / 1,000,000)
  // MB/s = (len * 1,000,000)/(usecs * 1,000,000)
  // MB/s = len/utime
  double median_rate = len / utime[med].InMicroseconds();
  double max_rate = len / utime[min].InMicroseconds();

  perf_test::PrintResult("len=", base::NumberToString(len), "median",
                         median_rate, "MB/s", true);
  perf_test::PrintResult("usecs=", base::NumberToString(total_test_time), "max",
                         max_rate, "MB/s", true);
}

TEST(SHA1PerfTest, Speed) {
  Timing(1024 * 1024U >> 1);
  Timing(1024 * 1024U >> 5);
  Timing(1024 * 1024U >> 6);
  Timing(1024 * 1024U >> 7);
}
