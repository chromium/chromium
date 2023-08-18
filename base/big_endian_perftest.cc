// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/big_endian.h"

#include <stdint.h>

#include "base/check.h"
#include "base/containers/span.h"
#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"

namespace base {
namespace {

constexpr size_t kSize = 128 * 1024 * 1024;
int64_t aligned_bytes[kSize / sizeof(int64_t)];
struct {
  int64_t aligment;
  char padding_to_cause_misalignment;
  char bytes[kSize];
} misaligned_bytes;

void DoNotOptimizeSpan(span<const char> range) {
  // ::benchmark::DoNotOptimize() generates quite large code, so instead of
  // calling it for every byte in the range, calculate `sum` which depends on
  // every byte in the range and then call DoNotOptimise() on that.
  int sum = 0;
  for (char c : range) {
    sum += c;
  }
  ::benchmark::DoNotOptimize(sum);
}

template <typename T>
inline void WriteBigEndianCommon(::benchmark::State& state, char* const start) {
  size_t offset = 0;
  T value = 0;
  for (auto _ : state) {
    WriteBigEndian(start + offset, value);
    offset += sizeof(T);
    static_assert(kSize % sizeof(T) == 0);
    if (offset == kSize) {
      offset = 0;
    }
    ++value;
  }
  DoNotOptimizeSpan({start, kSize});
}

template <typename T>
void BM_WriteBigEndianAligned(::benchmark::State& state) {
  char* const start = reinterpret_cast<char*>(aligned_bytes);
  CHECK(reinterpret_cast<uintptr_t>(start) % alignof(T) == 0);
  WriteBigEndianCommon<T>(state, start);
}

template <typename T>
void BM_WriteBigEndianMisaligned(::benchmark::State& state) {
  char* const start = misaligned_bytes.bytes;
  CHECK(reinterpret_cast<uintptr_t>(start) % alignof(T) != 0);
  WriteBigEndianCommon<T>(state, start);
}

template <typename T>
inline void ReadBigEndianCommon(::benchmark::State& state,
                                const uint8_t* const start) {
  size_t offset = 0;
  for (auto _ : state) {
    T value;
    ReadBigEndian(start + offset, &value);
    ::benchmark::DoNotOptimize(value);
    offset += sizeof(T);
    static_assert(kSize % sizeof(T) == 0);
    if (offset == kSize) {
      offset = 0;
    }
  }
}

template <typename T>
void BM_ReadBigEndianAligned(::benchmark::State& state) {
  const uint8_t* const start = reinterpret_cast<uint8_t*>(aligned_bytes);
  CHECK(reinterpret_cast<uintptr_t>(start) % alignof(T) == 0);
  ReadBigEndianCommon<T>(state, start);
}

template <typename T>
void BM_ReadBigEndianMisaligned(::benchmark::State& state) {
  const uint8_t* const start =
      reinterpret_cast<uint8_t*>(misaligned_bytes.bytes);
  CHECK(reinterpret_cast<uintptr_t>(start) % alignof(T) != 0);
  ReadBigEndianCommon<T>(state, start);
}

#define BENCHMARK_FOR_INT_TYPES(function)            \
  BENCHMARK(function<int16_t>)->MinWarmUpTime(1.0);  \
  BENCHMARK(function<uint16_t>)->MinWarmUpTime(1.0); \
  BENCHMARK(function<int32_t>)->MinWarmUpTime(1.0);  \
  BENCHMARK(function<uint32_t>)->MinWarmUpTime(1.0); \
  BENCHMARK(function<int64_t>)->MinWarmUpTime(1.0);  \
  BENCHMARK(function<uint64_t>)->MinWarmUpTime(1.0); \
  typedef int force_semicolon

BENCHMARK_FOR_INT_TYPES(BM_WriteBigEndianAligned);
BENCHMARK_FOR_INT_TYPES(BM_WriteBigEndianMisaligned);
BENCHMARK_FOR_INT_TYPES(BM_ReadBigEndianAligned);
BENCHMARK_FOR_INT_TYPES(BM_ReadBigEndianMisaligned);

#undef BENCHMARK_FOR_INT_TYPES

}  // namespace
}  // namespace base
