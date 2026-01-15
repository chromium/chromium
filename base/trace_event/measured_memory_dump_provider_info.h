// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_MEASURED_MEMORY_DUMP_PROVIDER_INFO_H_
#define BASE_TRACE_EVENT_MEASURED_MEMORY_DUMP_PROVIDER_INFO_H_

#include "base/base_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/timer/elapsed_timer.h"

namespace base::trace_event {

struct MemoryDumpProviderInfo;

// MemoryDumpManager owns a long-lived list of MemoryDumpProviderInfo objects,
// which each wrap a registered MemoryDumpProvider and add metadata. When a dump
// starts, it copies each MemoryDumpProviderInfo into a short-lived list held in
// MemoryDumpManager::ProcessMemoryDumpAsyncState, which is the list of
// providers to invoke during that specific memory dump.
//
// MeasuredMemoryDumpProviderInfo wraps the copied MemoryDumpProviderInfo with
// more metadata about the performance of that provider during the dump, for
// metrics. It's separate from MemoryDumpProviderInfo because this metadata is
// specific to the dump in progress, while MemoryDumpProviderInfo holds
// long-lived metadata.
//
// The MeasuredMemoryDumpProviderInfo wrapping a MemoryDumpProviderInfo instance
// is destroyed when that instance is discarded (because it's finished running,
// because MemoryDumpManager decides it shouldn't or can't run, or because it's
// still queued when the browser shuts down). At this point the destructor logs
// all the tracked metrics.
class BASE_EXPORT MeasuredMemoryDumpProviderInfo {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(MemoryDumpProviderStatus)
  enum class Status {
    // The provider is in the MemoryDumpManager::ProcessMemoryDumpAsyncState
    // queue, and hasn't started to be processed yet. If this status is logged,
    // the provider was still in the queue when MemoryDumpManager was destroyed.
    kQueued = 0,
    // The provider is at the front of the
    // MemoryDumpManager::ProcessMemoryDumpAsyncState queue. A task has been
    // posted to start processing the provider on another sequence. If this
    // status is logged, the posted task was dropped without running.
    kPosted = 1,
    // The provider is being skipped because it needs to run on another sequence
    // but the PostTask call failed.
    kFailedToPost = 2,
    // The provider is being skipped because the memory dump is in background
    // mode, and this provider is not allowed to run in the background.
    kIgnoredInBackground = 3,
    // The provider is being skipped because it's disabled.
    kIgnoredDisabled = 4,
    // The provider finished running OnMemoryDump and returned success.
    kDumpSucceeded = 5,
    // The provider finished running OnMemoryDump and returned failure.
    kDumpFailed = 6,
    kMaxValue = kDumpFailed,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/memory/enums.xml:MemoryDumpProviderStatus)

  // Default constructor for containers.
  MeasuredMemoryDumpProviderInfo();

  MeasuredMemoryDumpProviderInfo(
      scoped_refptr<MemoryDumpProviderInfo> provider_info,
      size_t num_following_providers);

  // Logs all metrics for the wrapped MemoryDumpProvider.
  ~MeasuredMemoryDumpProviderInfo();

  // Move-only.
  MeasuredMemoryDumpProviderInfo(const MeasuredMemoryDumpProviderInfo&) =
      delete;
  MeasuredMemoryDumpProviderInfo& operator=(
      const MeasuredMemoryDumpProviderInfo&) = delete;
  MeasuredMemoryDumpProviderInfo(MeasuredMemoryDumpProviderInfo&&);
  MeasuredMemoryDumpProviderInfo& operator=(MeasuredMemoryDumpProviderInfo&&);

  // Returns the wrapped MemoryDumpProviderInfo, which in turn wraps a
  // MemoryDumpProvider.
  MemoryDumpProviderInfo* provider_info() { return provider_info_.get(); }
  const MemoryDumpProviderInfo* provider_info() const {
    return provider_info_.get();
  }

  // Returns the number of providers that are queued to run after this one.
  size_t num_following_providers() const { return num_following_providers_; }

  // Updates the current status of the provider. The status begins as kQueued,
  // and MemoryDumpManager should update it whenever it moves the
  // MemoryDumpProviderInfo to a new state.
  void SetStatus(Status status) { status_ = status; }

 private:
  scoped_refptr<MemoryDumpProviderInfo> provider_info_;
  size_t num_following_providers_;
  Status status_ = Status::kQueued;

  // Measures the time between the MemoryDumpProvider being placed into the
  // queue when a memory dump starts, and the MeasuredMemoryDumpProviderInfo
  // being destroyed. This includes the time the MemoryDumpProvider spent in the
  // queue (while other providers were running), and the time the provider was
  // running (if `status_` is kDumpSucceeded or kDumpFailed).
  base::ElapsedLiveTimer elapsed_timer_;
};

}  // namespace base::trace_event

#endif  // BASE_TRACE_EVENT_MEASURED_MEMORY_DUMP_PROVIDER_INFO_H_
