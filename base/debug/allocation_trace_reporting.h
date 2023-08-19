// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_DEBUG_ALLOCATION_TRACE_REPORTING_H_
#define BASE_DEBUG_ALLOCATION_TRACE_REPORTING_H_

#include <memory>

#include "base/base_export.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"

namespace base::debug::tracer {
struct AllocationTraceRecorder;
class SequenceSpecificData;

// The reporting for AllocationTraceRecorder starts a reporting task which
// periodically fetches the current statistics from the passed
// AllocationcTraceRecorder and prints these.
class BASE_EXPORT AllocationTraceRecorderReporter {
 public:
  // Create a new reporting instance. This does not start any reporting
  // whatsoever.
  AllocationTraceRecorderReporter();

  // Destroy a reporting instance and stop any reporting that might be running.
  ~AllocationTraceRecorderReporter();

  // Start reporting for the given |recorder|. Log messages will contain
  // information on the |process_type| to allow distinction between the various
  // processes. Messages will be logged using the passed |severity| at the given
  // |interval|. The |recorder| must be valid until the reporting is stopped by
  // calling Stop() or by destructing the reporter.
  void Start(base::debug::tracer::AllocationTraceRecorder& recorder,
             base::StringPiece process_type,
             base::TimeDelta interval,
             logging::LogSeverity severity);

  void Stop();

 private:
  std::unique_ptr<base::SequenceBound<SequenceSpecificData>>
      reporting_sequence_;
};

}  // namespace base::debug::tracer
#endif  // BASE_DEBUG_ALLOCATION_TRACE_REPORTING_H_
