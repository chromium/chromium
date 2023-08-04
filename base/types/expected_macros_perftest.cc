// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/types/expected_macros.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/types/expected.h"
#include "third_party/abseil-cpp/absl/base/attributes.h"
#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"

namespace base {
namespace {

// Basis for `RETURN_IF_ERROR` and `ASSIGN_OR_RETURN` benchmarks.  Derived
// classes override `LoopAgain` with the macro invocation(s).
class ReturnLoop {
 public:
  using ReturnType = expected<int, std::string>;

  explicit ReturnLoop(ReturnType return_value)
      : value_(std::move(return_value)) {}
  virtual ~ReturnLoop() = default;

  ABSL_ATTRIBUTE_NO_TAIL_CALL ReturnType Loop(size_t* ops) {
    if (!*ops) {
      return value_;
    }
    return LoopAgain(ops);
  }

  ReturnType return_value() const { return value_; }

 private:
  virtual ReturnType LoopAgain(size_t* ops) = 0;

  const ReturnType value_;
};

class ReturnIfErrorLoop : public ReturnLoop {
 public:
  using ReturnLoop::ReturnLoop;

 private:
  ReturnType LoopAgain(size_t* ops) override {
    --*ops;
    RETURN_IF_ERROR(Loop(ops));
    return 0;
  }
};

class ReturnIfErrorWithAnnotateLoop : public ReturnLoop {
 public:
  using ReturnLoop::ReturnLoop;

 private:
  ReturnType LoopAgain(size_t* ops) override {
    --*ops;
    RETURN_IF_ERROR(Loop(ops), [](std::string e) {
      return e + "The quick brown fox jumped over the lazy dog.";
    });
    return 0;
  }
};

class AssignOrReturnLoop : public ReturnLoop {
 public:
  using ReturnLoop::ReturnLoop;

 private:
  ReturnType LoopAgain(size_t* ops) override {
    --*ops;
    ASSIGN_OR_RETURN(const int result, Loop(ops));
    return result;
  }
};

class AssignOrReturnAnnotateLoop : public ReturnLoop {
 public:
  using ReturnLoop::ReturnLoop;

 private:
  ReturnType LoopAgain(size_t* ops) override {
    --*ops;
    ASSIGN_OR_RETURN(const int result, Loop(ops), [](std::string e) {
      return e + "The quick brown fox jumped over the lazy dog.";
    });
    return result;
  }
};

std::string BenchmarkError() {
  // This error message is intended to be long enough to guarantee external
  // memory allocation in `std::string`.
  return "The quick brown fox jumped over the lazy dog.";
}

// Drive a benchmark loop.  `T` is intended to be a `ReturnLoop` (above).
template <class T>
void BenchmarkLoop(T* driver, ::benchmark::State* state) {
  // We benchmark 8 macro invocations (stack depth) per loop.  This
  // amortizes one time costs (e.g. building the initial error value)
  // across what we actually care about.
  constexpr int kMaxOps = 8;
  while (state->KeepRunningBatch(kMaxOps)) {
    size_t ops = kMaxOps;
    auto ret = driver->Loop(&ops);
    ::benchmark::DoNotOptimize(ret);
  }
}

// TODO(https://crbug.com/1404759): Update test-driving scripts to control
// google_benchmark correctly and parse its output, so that these benchmarks'
// results are included in bot output.

void BM_ReturnIfError_Ok(::benchmark::State& state) {
  ReturnIfErrorLoop loop(1);
  BenchmarkLoop(&loop, &state);
}
BENCHMARK(BM_ReturnIfError_Ok);

void BM_ReturnIfError_Error(::benchmark::State& state) {
  ReturnIfErrorLoop loop{unexpected(BenchmarkError())};
  BenchmarkLoop(&loop, &state);
}
BENCHMARK(BM_ReturnIfError_Error);

void BM_ReturnIfError_Annotate_Ok(::benchmark::State& state) {
  ReturnIfErrorWithAnnotateLoop loop(1);
  BenchmarkLoop(&loop, &state);
}
BENCHMARK(BM_ReturnIfError_Annotate_Ok);

void BM_ReturnIfError_Annotate_Error(::benchmark::State& state) {
  ReturnIfErrorWithAnnotateLoop loop{unexpected(BenchmarkError())};
  BenchmarkLoop(&loop, &state);
}
BENCHMARK(BM_ReturnIfError_Annotate_Error);

void BM_AssignOrReturn_Ok(::benchmark::State& state) {
  AssignOrReturnLoop loop(1);
  BenchmarkLoop(&loop, &state);
}
BENCHMARK(BM_AssignOrReturn_Ok);

void BM_AssignOrReturn_Error(::benchmark::State& state) {
  AssignOrReturnLoop loop{unexpected(BenchmarkError())};
  BenchmarkLoop(&loop, &state);
}
BENCHMARK(BM_AssignOrReturn_Error);

void BM_AssignOrReturn_Annotate_Ok(::benchmark::State& state) {
  AssignOrReturnAnnotateLoop loop(1);
  BenchmarkLoop(&loop, &state);
}
BENCHMARK(BM_AssignOrReturn_Annotate_Ok);

void BM_AssignOrReturn_Annotate_Error(::benchmark::State& state) {
  AssignOrReturnAnnotateLoop loop{unexpected(BenchmarkError())};
  BenchmarkLoop(&loop, &state);
}
BENCHMARK(BM_AssignOrReturn_Annotate_Error);

}  // namespace
}  // namespace base
