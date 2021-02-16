// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/hash/sha1.h"

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

#include "base/hash/hash.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace base {
namespace {

void Sha1Hash(void* data, size_t size) {
  unsigned char digest[kSHA1Length];
  memset(digest, 0, kSHA1Length);
  SHA1HashBytes(reinterpret_cast<uint8_t*>(data), size, digest);
}

void FastHash(void* data, size_t size) {
  base::Hash(reinterpret_cast<uint8_t*>(data), size);
}

void RunTest(const char* hash_name,
             void (*hash)(void*, size_t),
             const size_t len) {
  constexpr char kMetricRuntime[] = "runtime";
  constexpr char kMetricThroughput[] = "throughput";
  // Histograms automatically calculate mean, min, max, and standard deviation,
  // but not median, so have a separate metric for a manually calculated median.
  constexpr char kMetricMedianThroughput[] = "median_throughput";

  perf_test::PerfResultReporter reporter(hash_name,
                                         NumberToString(len) + "_bytes");
  reporter.RegisterImportantMetric(kMetricRuntime, "us");
  reporter.RegisterImportantMetric(kMetricThroughput, "bytesPerSecond");
  reporter.RegisterImportantMetric(kMetricMedianThroughput, "bytesPerSecond");

  constexpr int kNumRuns = 111;
  std::vector<TimeDelta> utime(kNumRuns);
  TimeDelta total_test_time;
  {
    std::vector<uint8_t> buf(len);
    RandBytes(buf.data(), len);

    for (int i = 0; i < kNumRuns; ++i) {
      const auto start = TimeTicks::Now();
      hash(buf.data(), len);
      utime[i] = TimeTicks::Now() - start;
      total_test_time += utime[i];
    }
    ranges::sort(utime);
  }

  reporter.AddResult(kMetricRuntime, total_test_time.InMicrosecondsF());

  // Simply dividing len by utime gets us MB/s, but we need B/s.
  // MB/s = (len / (bytes/megabytes)) / (usecs / usecs/sec)
  // MB/s = (len / 1,000,000)/(usecs / 1,000,000)
  // MB/s = (len * 1,000,000)/(usecs * 1,000,000)
  // MB/s = len/utime
  constexpr int kBytesPerMegabyte = 1'000'000;
  const auto rate = [len](TimeDelta t) {
    return kBytesPerMegabyte * (len / t.InMicrosecondsF());
  };

  reporter.AddResult(kMetricMedianThroughput, rate(utime[kNumRuns / 2]));

  // Convert to a comma-separated string so we can report every data point.
  std::vector<std::string> rate_strings(utime.size());
  ranges::transform(utime, rate_strings.begin(),
                    [rate](const auto& t) { return NumberToString(rate(t)); });
  reporter.AddResultList(kMetricThroughput, JoinString(rate_strings, ","));
}

}  // namespace

TEST(SHA1PerfTest, Speed) {
  for (int shift : {1, 5, 6, 7}) {
    RunTest("SHA1.", Sha1Hash, 1024 * 1024U >> shift);
  }
}

TEST(HashPerfTest, Speed) {
  for (int shift : {1, 5, 6, 7}) {
    RunTest("FastHash.", FastHash, 1024 * 1024U >> shift);
  }
}

}  // namespace base
