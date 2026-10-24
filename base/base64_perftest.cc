// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "base/test/insecure_random_generator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"

namespace base {
namespace {

using EncodeAppendFn = void (*)(span<const uint8_t>, std::string*);

std::vector<uint8_t> GenerateInput(size_t size) {
  test::InsecureRandomGenerator rand_gen;
  rand_gen.ReseedForTesting(42);
  std::vector<uint8_t> input(size);
  std::ranges::generate(
      input, [&]() { return static_cast<uint8_t>(rand_gen.RandUint32()); });
  return input;
}

template <EncodeAppendFn EncodeFn>
void RunBase64Encode(::benchmark::State& state) {
  const std::vector<uint8_t> input =
      GenerateInput(checked_cast<size_t>(state.range(0)));
  for (auto _ : state) {
    std::string encoded;
    EncodeFn(input, &encoded);
    ::benchmark::DoNotOptimize(encoded);
  }
  state.SetBytesProcessed(state.iterations() * state.range(0));
}

void BM_Base64Encode_ModpB64(::benchmark::State& state) {
  RunBase64Encode<&Base64EncodeAppendModpB64ForTesting>(state);
}

void BM_Base64Encode_Simdutf(::benchmark::State& state) {
  RunBase64Encode<&Base64EncodeAppendSimdutfForTesting>(state);
}

void AddBenchmarkArgs(::benchmark::Benchmark* benchmark) {
  benchmark->Arg(64)->Arg(1024)->Arg(64 * 1024)->Arg(1024 * 1024);
}

// Register the benchmarks as a GTest test. This allows using legacy
// `--gtest_filter` and `--gtest_list_tests`.
// TODO(https://crbug.com/40251982): Clean this up after transitioning to
// `--benchmark_filter` and `--benchmark_list_tests`.
TEST(Base64PerfTest, Encode) {
  BENCHMARK(BM_Base64Encode_ModpB64)->Apply(AddBenchmarkArgs);
  BENCHMARK(BM_Base64Encode_Simdutf)->Apply(AddBenchmarkArgs);
}

}  // namespace
}  // namespace base
