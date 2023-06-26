// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/allocation_trace_reporting.h"

#include <functional>
#include <ios>
#include <string>
#include <utility>

#include "base/debug/allocation_trace.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace base::debug::tracer {
class SequenceSpecificData {
 public:
  SequenceSpecificData(AllocationTraceRecorder& recorder,
                       std::string process_type,
                       base::TimeDelta interval,
                       logging::LogSeverity severity);

 private:
  void LogRecorderStatistics();

  base::RepeatingTimer timer_;

  // Data of the report itself.
  base::raw_ptr<AllocationTraceRecorder> recorder_;
  std::string process_type_;
  logging::LogSeverity severity_;
};

SequenceSpecificData::SequenceSpecificData(AllocationTraceRecorder& recorder,
                                           std::string process_type,
                                           base::TimeDelta interval,
                                           logging::LogSeverity severity)
    : recorder_(&recorder),
      process_type_(std::move(process_type)),
      severity_(severity) {
  timer_.Start(FROM_HERE, interval,
               base::BindRepeating(&SequenceSpecificData::LogRecorderStatistics,
                                   base::Unretained(this)));
}

void SequenceSpecificData::LogRecorderStatistics() {
  const auto recorder_stats = recorder_->GetRecorderStatistics();
  const float collision_ratio =
      recorder_stats.total_number_of_allocations != 0
          ? static_cast<float>(recorder_stats.total_number_of_collisions) /
                recorder_stats.total_number_of_allocations
          : 0;

  logging::LogMessage(__FILE__, __LINE__, severity_).stream()
      << "process-type=" << process_type_ << ", number_of_allocations="
      << recorder_stats.total_number_of_allocations
      << ", number_of_collisions=" << recorder_stats.total_number_of_collisions
      << ", collision_ratio = " << std::fixed << collision_ratio;
}

AllocationTraceRecorderReporter::AllocationTraceRecorderReporter() = default;
AllocationTraceRecorderReporter::~AllocationTraceRecorderReporter() = default;

void AllocationTraceRecorderReporter::Start(AllocationTraceRecorder& recorder,
                                            base::StringPiece process_type,
                                            base::TimeDelta interval,
                                            logging::LogSeverity severity) {
  // We should not retain a reference to StringPiece to avoid use-after-free
  // since the bound object is constructed asynchronously on the bound sequence.
  reporting_sequence_ =
      std::make_unique<base::SequenceBound<SequenceSpecificData>>(
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
               base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}),
          std::ref(recorder), std::string(process_type), interval, severity);
}

void AllocationTraceRecorderReporter::Stop() {
  // The destructor of SequenceBound will properly destroy SequenceSpecificData,
  // whose destructor in turn will stop the timer.
  reporting_sequence_.reset();
}

}  // namespace base::debug::tracer
