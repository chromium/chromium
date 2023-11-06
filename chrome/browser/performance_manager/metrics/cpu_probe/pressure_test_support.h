// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_CPU_PROBE_PRESSURE_TEST_SUPPORT_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_CPU_PROBE_PRESSURE_TEST_SUPPORT_H_

#include <stdint.h>

#include <type_traits>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/test/test_future.h"
#include "base/thread_annotations.h"
#include "chrome/browser/performance_manager/metrics/cpu_probe/cpu_probe.h"
#include "chrome/browser/performance_manager/metrics/cpu_probe/pressure_sample.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace performance_manager::metrics {

// Test double for platform specific CpuProbe that stores the PressureSample in
// a TestFuture.
template <typename T,
          typename = std::enable_if_t<std::is_base_of_v<CpuProbe, T>>>
class FakePlatformCpuProbe : public T {
 public:
  template <typename... Args>
  explicit FakePlatformCpuProbe(Args&&... args)
      : T(std::forward<Args>(args)...) {}
  ~FakePlatformCpuProbe() override = default;

  // Tests the internals of each platform CPU probe by calling Update() directly
  // instead of using the public interface.
  absl::optional<PressureSample> UpdateAndWaitForSample() {
    T::Update(sample_.GetCallback());
    // Blocks until the sample callback is invoked.
    return sample_.Take();
  }

 private:
  base::test::TestFuture<absl::optional<PressureSample>> sample_;
};

// Test double for CpuProbe that always returns a predetermined value.
class FakeCpuProbe final : public CpuProbe {
 public:
  FakeCpuProbe();
  ~FakeCpuProbe() final;

  // CpuProbe implementation.
  void Update(SampleCallback callback) final;
  base::WeakPtr<CpuProbe> GetWeakPtr() final;

  // Can be called from any thread.
  void SetLastSample(absl::optional<PressureSample> sample);

 private:
  base::Lock lock_;
  absl::optional<PressureSample> last_sample_ GUARDED_BY_CONTEXT(lock_);

  base::WeakPtrFactory<FakeCpuProbe> weak_factory_{this};
};

// Test double for CpuProbe that produces a different value after every
// Update().
class StreamingCpuProbe final : public CpuProbe {
 public:
  StreamingCpuProbe(std::vector<PressureSample>, base::OnceClosure);

  ~StreamingCpuProbe() final;

  // CpuProbe implementation.
  void Update(SampleCallback callback) final;
  base::WeakPtr<CpuProbe> GetWeakPtr() final;

 private:
  std::vector<PressureSample> samples_ GUARDED_BY_CONTEXT(sequence_checker_);
  size_t sample_index_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  // This closure is called on an Update() call after the expected number of
  // samples has been taken by PressureSampler.
  base::OnceClosure done_callback_;

  base::WeakPtrFactory<StreamingCpuProbe> weak_factory_{this};
};

}  // namespace performance_manager::metrics

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_CPU_PROBE_PRESSURE_TEST_SUPPORT_H_
