// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_PCSCAN_SCHEDULING_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_PCSCAN_SCHEDULING_H_

#include <atomic>
#include <cstdint>

#include "base/base_export.h"
#include "base/compiler_specific.h"

namespace base {
namespace internal {

class PCScanScheduler;

struct QuarantineData final {
  static constexpr size_t kQuarantineSizeMinLimit = 1 * 1024 * 1024;

  inline constexpr QuarantineData();

  bool MinimumScanningThresholdReached() const {
    return current_size.load(std::memory_order_relaxed) >
           kQuarantineSizeMinLimit;
  }

  std::atomic<size_t> current_size{0u};
  std::atomic<size_t> size_limit{kQuarantineSizeMinLimit};
  std::atomic<size_t> epoch{0u};
  size_t last_size{0u};
};

class PCScanSchedulingBackend {
 public:
  explicit inline constexpr PCScanSchedulingBackend(PCScanScheduler&);
  // No virtual destructor to allow constant initialization of PCScan as
  // static global which directly embeds LimitBackend as default backend.

  PCScanSchedulingBackend(const PCScanSchedulingBackend&) = delete;
  PCScanSchedulingBackend& operator=(const PCScanSchedulingBackend&) = delete;

  inline QuarantineData& GetQuarantineData();

  // Invoked when the limit in PCScanScheduler is reached. Returning true
  // signals the caller to invoke a scan.
  virtual bool LimitReached() = 0;

  // Invoked on starting a scan.
  virtual void ScanStarted() = 0;

  // Invoked at the end of a scan to compute a new limit.
  virtual void UpdateScheduleAfterScan(size_t survived_bytes,
                                       size_t heap_size) = 0;

 protected:
  PCScanScheduler& scheduler_;
};

// Scheduling backend that just considers a single hard limit.
class BASE_EXPORT LimitBackend final : public PCScanSchedulingBackend {
 public:
  static constexpr double kQuarantineSizeFraction = 0.1;

  explicit inline constexpr LimitBackend(PCScanScheduler&);

  bool LimitReached() final;
  void ScanStarted() final;
  void UpdateScheduleAfterScan(size_t, size_t) final;
};

// The scheduler that is embedded in the PCSCan frontend which requires a fast
// path for freeing objects. The scheduler holds data needed to invoke a
// `PCScanSchedulingBackend` upon hitting a limit. The backend implements
// the actual scheduling strategy and is in charge of maintaining limits.
class BASE_EXPORT PCScanScheduler final {
 public:
  inline constexpr PCScanScheduler();

  PCScanScheduler(const PCScanScheduler&) = delete;
  PCScanScheduler& operator=(const PCScanScheduler&) = delete;

  // Account freed `bytes`. Returns true if scan should be triggered
  // immediately, and false otherwise.
  ALWAYS_INLINE bool AccountFreed(size_t bytes);

  size_t epoch() const {
    return quarantine_data_.epoch.load(std::memory_order_relaxed);
  }

  // Sets a new scheduling backend that should be used by the scanner.
  void SetNewSchedulingBackend(PCScanSchedulingBackend&);

  PCScanSchedulingBackend& scheduling_backend() { return *backend_; }
  const PCScanSchedulingBackend& scheduling_backend() const {
    return *backend_;
  }

 private:
  QuarantineData quarantine_data_{};
  // The default backend used is a simple LimitBackend that just triggers scan
  // on reaching a hard limit.
  LimitBackend default_scheduling_backend_{*this};
  PCScanSchedulingBackend* backend_ = &default_scheduling_backend_;

  friend PCScanSchedulingBackend;
};

// To please Chromium's clang plugin.
constexpr PCScanScheduler::PCScanScheduler() = default;
constexpr QuarantineData::QuarantineData() = default;

constexpr PCScanSchedulingBackend::PCScanSchedulingBackend(
    PCScanScheduler& scheduler)
    : scheduler_(scheduler) {}

QuarantineData& PCScanSchedulingBackend::GetQuarantineData() {
  return scheduler_.quarantine_data_;
}

constexpr LimitBackend::LimitBackend(PCScanScheduler& scheduler)
    : PCScanSchedulingBackend(scheduler) {}

bool PCScanScheduler::AccountFreed(size_t size) {
  const size_t size_before =
      quarantine_data_.current_size.fetch_add(size, std::memory_order_relaxed);
  return (size_before + size >
          quarantine_data_.size_limit.load(std::memory_order_relaxed)) &&
         backend_->LimitReached();
}

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_PCSCAN_SCHEDULING_H_
