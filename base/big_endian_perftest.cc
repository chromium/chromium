// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/big_endian.h"

#include <stdint.h>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/numerics/byte_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
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

void DoNotOptimizeSpan(span<const uint8_t> range) {
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
inline void WriteBigEndianCommon(::benchmark::State& state,
                                 span<uint8_t, kSize> buffer) {
  size_t offset = 0u;
  auto value = T{0};
  for (auto _ : state) {
    if constexpr (sizeof(T) == 1) {
      buffer.subspan(offset).first<sizeof(T)>().copy_from(U8ToBigEndian(value));
    } else if constexpr (sizeof(T) == 2) {
      buffer.subspan(offset).first<sizeof(T)>().copy_from(
          U16ToBigEndian(value));
    } else if constexpr (sizeof(T) == 4) {
      buffer.subspan(offset).first<sizeof(T)>().copy_from(
          U32ToBigEndian(value));
    } else {
      static_assert(sizeof(T) == 8);
      buffer.subspan(offset).first<sizeof(T)>().copy_from(
          U64ToBigEndian(value));
    }
    offset += sizeof(T);
    static_assert(kSize % sizeof(T) == 0u);
    if (offset == kSize) {
      offset = 0;
    }
    ++value;
  }
  DoNotOptimizeSpan(buffer);
}

template <typename T>
void BM_WriteBigEndianAligned(::benchmark::State& state) {
  span<uint8_t, kSize> buffer = as_writable_byte_span(aligned_bytes);
  CHECK(reinterpret_cast<uintptr_t>(buffer.data()) % alignof(T) == 0u);
  WriteBigEndianCommon<T>(state, buffer);
}

template <typename T>
void BM_WriteBigEndianMisaligned(::benchmark::State& state) {
  span<uint8_t, kSize> buffer = as_writable_byte_span(misaligned_bytes.bytes);
  CHECK(reinterpret_cast<uintptr_t>(buffer.data()) % alignof(T) != 0u);
  WriteBigEndianCommon<T>(state, buffer);
}

template <typename T>
inline void ReadBigEndianCommon(::benchmark::State& state,
                                span<const uint8_t, kSize> buffer) {
  size_t offset = 0;
  for (auto _ : state) {
    T value;
    if constexpr (sizeof(T) == 1) {
      value = U8FromBigEndian(buffer.subspan(offset).first<sizeof(T)>());
    } else if constexpr (sizeof(T) == 2) {
      value = U16FromBigEndian(buffer.subspan(offset).first<sizeof(T)>());
    } else if constexpr (sizeof(T) == 4) {
      value = U32FromBigEndian(buffer.subspan(offset).first<sizeof(T)>());
    } else {
      static_assert(sizeof(T) == 8);
      value = U64FromBigEndian(buffer.subspan(offset).first<sizeof(T)>());
    }
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
  span<const uint8_t, kSize> buffer = as_byte_span(aligned_bytes);
  CHECK(reinterpret_cast<uintptr_t>(buffer.data()) % alignof(T) == 0);
  ReadBigEndianCommon<T>(state, buffer);
}

template <typename T>
void BM_ReadBigEndianMisaligned(::benchmark::State& state) {
  span<const uint8_t, kSize> buffer = as_byte_span(misaligned_bytes.bytes);
  CHECK(reinterpret_cast<uintptr_t>(buffer.data()) % alignof(T) != 0);
  ReadBigEndianCommon<T>(state, buffer);
}

#define BENCHMARK_FOR_INT_TYPES(function)            \
  BENCHMARK(function<int16_t>)->MinWarmUpTime(1.0);  \
  BENCHMARK(function<uint16_t>)->MinWarmUpTime(1.0); \
  BENCHMARK(function<int32_t>)->MinWarmUpTime(1.0);  \
  BENCHMARK(function<uint32_t>)->MinWarmUpTime(1.0); \
  BENCHMARK(function<int64_t>)->MinWarmUpTime(1.0);  \
  BENCHMARK(function<uint64_t>)->MinWarmUpTime(1.0);

// Register the benchmarks as a GTest test. This allows using legacy
// --gtest_filter and --gtest_list_tests.
// TODO(https://crbug.com/40251982): Clean this up after transitioning to
// --benchmark_filter and --benchmark_list_tests.
TEST(BigEndianPerfTest, All) {
  BENCHMARK_FOR_INT_TYPES(BM_WriteBigEndianAligned);
  BENCHMARK_FOR_INT_TYPES(BM_WriteBigEndianMisaligned);
  BENCHMARK_FOR_INT_TYPES(BM_ReadBigEndianAligned);
  BENCHMARK_FOR_INT_TYPES(BM_ReadBigEndianMisaligned);
}

#undef BENCHMARK_FOR_INT_TYPES

}  // namespace
}  // namespace base
