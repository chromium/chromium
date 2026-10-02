// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_THREADING_PLATFORM_THREAD_INTERNAL_H_
#define BASE_THREADING_PLATFORM_THREAD_INTERNAL_H_

#include <array>
#include <cstdint>
#include <optional>

#include "base/base_export.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/thread_type.h"

namespace base::internal {

// Manages the thread's ThreadType by allowing it to be raised via
// leases. The effective thread type is the maximum of:
// 1. The default thread type (set via SetDefault()).
// 2. The highest active raise lease (managed via Acquire/DropRaiseLease()).
//
// This allows a thread to have a baseline priority while temporarily boosting
// it for critical sections or based on workload requirements, without losing
// track of the original priority.
class BASE_EXPORT ThreadTypeManager {
 public:
  class RaiseLeases {
   public:
    void Acquire(ThreadType thread_type);
    void Drop(ThreadType thread_type);
    std::optional<ThreadType> GetHighestLease() const;

   private:
    // Contains the number of active leases for each thread type. The positions
    // correspond to the enum values of ThreadType.
    std::array<uint32_t, static_cast<int>(ThreadType::kMaxValue) + 1> leases =
        {};

    // A bitmask of the thread types that have active leases. This is used to
    // find the highest active lease thread type via Log2Floor.
    uint32_t bitmask = 0;
  };
  ThreadTypeManager() = default;
  virtual ~ThreadTypeManager() = default;
  ThreadTypeManager(const ThreadTypeManager&) = delete;
  ThreadTypeManager& operator=(const ThreadTypeManager&) = delete;

  void SetDefault(ThreadType type);
  ThreadType GetCurrent() const;
  void MaybeUpdate();
  void AcquireRaiseLease(ThreadType type);
  void DropRaiseLease(ThreadType type);
  bool HasLeases() const;

 private:
  virtual void SetCurrentThreadTypeImpl(ThreadType thread_type,
                                        MessagePumpType pump_type_hint);

  // `default_thread_type_` can be nullopt to be able to express the state
  // where neither SetDefault has been used, nor any leases have been created.
  // In this state, the thread's type isn't managed by ThreadTypeManager but is
  // what the OS has assigned for the thread (which may not be expressible as a
  // Chromium ThreadType). From this state, the first lease or SetDefault call
  // will apply the thread's initial type.
  std::optional<ThreadType> default_thread_type_;

  // The thread type last applied to the thread. This is nullopt if no thread
  // type has ever been applied by SetDefault or leases.
  std::optional<ThreadType> effective_thread_type_;
  RaiseLeases raise_leases_;
};

}  // namespace base::internal

#endif  // BASE_THREADING_PLATFORM_THREAD_INTERNAL_H_
