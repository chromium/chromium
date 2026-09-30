// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/etw_allocation_observer_win.h"

#include <windows.h>

#include <evntrace.h>
#include <stddef.h>

#include "base/allocator/dispatcher/subsystem.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"

namespace base::allocator {

namespace {

// TraceLogging writes the event name and the field names into every event
// record, and a busy renderer produces millions of allocations per second, so
// both are deliberately terse. "PA" marks the events that come from direct
// PartitionAlloc use (Blink partitions and friends) rather than malloc/new.
constexpr EVENT_DESCRIPTOR kAllocEvent =
    TlmEventDescriptor(TRACE_LEVEL_VERBOSE, kEtwAllocationKeyword);
constexpr EVENT_DESCRIPTOR kFreeEvent =
    TlmEventDescriptor(TRACE_LEVEL_VERBOSE, kEtwFreeKeyword);

static_assert((kEtwProcessTypeKeywords &
               (kEtwAllocationKeyword | kEtwFreeKeyword)) == 0,
              "process type keywords must not overlap the event keywords");

// These are defined in content/public/common/content_switches.h, which is not
// accessible in ::base. They must be kept in sync.
constexpr std::string_view kRendererProcess = "renderer";
constexpr std::string_view kGpuProcess = "gpu-process";
constexpr std::string_view kUtilityProcess = "utility";

// EventWrite() is not documented to allocate, but re-entering the allocation
// path from within an allocation hook would recurse without bound, so pay for
// a thread-local flag on the (already slow) writing path.
constinit thread_local bool g_writing_event = false;

class ScopedWriteGuard {
 public:
  ScopedWriteGuard() : allowed_(!g_writing_event) { g_writing_event = true; }

  ScopedWriteGuard(const ScopedWriteGuard&) = delete;
  ScopedWriteGuard& operator=(const ScopedWriteGuard&) = delete;

  ~ScopedWriteGuard() {
    if (allowed_) {
      g_writing_event = false;
    }
  }

  explicit operator bool() const { return allowed_; }

 private:
  const bool allowed_;
};

}  // namespace

EtwProcessTypeKeyword GetEtwProcessTypeKeyword(std::string_view process_type) {
  if (process_type.empty()) {
    return EtwProcessTypeKeyword::kBrowser;
  }
  if (process_type == kRendererProcess) {
    return EtwProcessTypeKeyword::kRenderer;
  }
  if (process_type == kGpuProcess) {
    return EtwProcessTypeKeyword::kGpu;
  }
  if (process_type == kUtilityProcess) {
    return EtwProcessTypeKeyword::kUtility;
  }
  return EtwProcessTypeKeyword::kOther;
}

EtwAllocationObserver::EtwAllocationObserver() = default;

EtwAllocationObserver::~EtwAllocationObserver() = default;

// static
EtwAllocationObserver* EtwAllocationObserver::Get() {
  static base::NoDestructor<EtwAllocationObserver> instance;
  return instance.get();
}

void EtwAllocationObserver::Register(std::string_view process_type) {
  // A session that is already listening can enable the provider before
  // provider_.Register() returns, so this has to be known first.
  process_type_ = GetEtwProcessTypeKeyword(process_type);

  provider_.Register(
      kEtwProviderName, kEtwProviderGuid,
      BindRepeating(&EtwAllocationObserver::OnProviderStateChanged,
                    Unretained(this)));

  // A session may already have been listening before this process started.
  if (provider_.IsEnabled()) {
    OnProviderStateChanged(TlmProvider::EventControlCode::kEnableProvider);
  }
}

void EtwAllocationObserver::SetEnabledKeywordsForTesting(uint64_t keywords) {
  // Not a trivial accessor: this opens the event writing path without a
  // tracing session having asked for it.
  CHECK_IS_TEST();
  enabled_keywords_.store(keywords, std::memory_order_relaxed);
}

void EtwAllocationObserver::OnProviderStateChanged(
    TlmProvider::EventControlCode event) {
  const uint64_t keywords =
      event == TlmProvider::EventControlCode::kDisableProvider
          ? 0u
          : FilterKeywordsForProcessType(provider_.keyword_any(),
                                         process_type_);
  enabled_keywords_.store(keywords, std::memory_order_relaxed);
}

// static
uint64_t EtwAllocationObserver::FilterKeywordsForProcessType(
    uint64_t keywords,
    EtwProcessTypeKeyword process_type) {
  // Sessions that name no process type at all trace every process.
  if ((keywords & kEtwProcessTypeKeywords) == 0 ||
      (keywords & static_cast<uint64_t>(process_type)) != 0) {
    return keywords;
  }
  return 0;
}

NOINLINE void EtwAllocationObserver::WriteAllocationEvent(
    const dispatcher::AllocationNotificationData& notification_data) {
  ScopedWriteGuard guard;
  if (!guard) {
    return;
  }

  const uint64_t address =
      reinterpret_cast<uintptr_t>(notification_data.address());
  const uint64_t size = notification_data.size();

  if (notification_data.allocation_subsystem() ==
      dispatcher::AllocationSubsystem::kPartitionAllocator) {
    // PartitionAlloc knows the partition a typed allocation came from, which is
    // attribution the call stack alone does not always provide.
    const char* const type_name = notification_data.type_name();
    provider_.WriteEvent(
        kEtwPartitionAllocationEventName, kAllocEvent,
        TlmUInt64Field("Addr", address), TlmUInt64Field("Size", size),
        TlmUtf8StringField("Type", type_name ? type_name : ""));
    return;
  }

  provider_.WriteEvent(kEtwAllocationEventName, kAllocEvent,
                       TlmUInt64Field("Addr", address),
                       TlmUInt64Field("Size", size));
}

NOINLINE void EtwAllocationObserver::WriteFreeEvent(
    const dispatcher::FreeNotificationData& notification_data) {
  ScopedWriteGuard guard;
  if (!guard) {
    return;
  }

  const uint64_t address =
      reinterpret_cast<uintptr_t>(notification_data.address());

  // Frees carry no size: consumers have to remember it from the matching
  // allocation, exactly as they already do for the Windows heap provider.
  if (notification_data.allocation_subsystem() ==
      dispatcher::AllocationSubsystem::kPartitionAllocator) {
    provider_.WriteEvent(kEtwPartitionFreeEventName, kFreeEvent,
                         TlmUInt64Field("Addr", address));
    return;
  }

  provider_.WriteEvent(kEtwFreeEventName, kFreeEvent,
                       TlmUInt64Field("Addr", address));
}

}  // namespace base::allocator
