// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#ifndef BASE_TRACE_EVENT_TRACE_EVENT_IMPL_H_
#define BASE_TRACE_EVENT_TRACE_EVENT_IMPL_H_

#include <stdint.h>

#include <iosfwd>
#include <string>

#include "base/base_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/process/process_handle.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_arguments.h"

namespace base::trace_event {

void BASE_EXPORT SetPerfettoInitializedForTesting();
bool BASE_EXPORT IsPerfettoInitializedForTesting();
void BASE_EXPORT InitializeInProcessPerfettoBackend();

using ArgumentNameFilterPredicate =
    base::RepeatingCallback<bool(const char* arg_name)>;

using ArgumentFilterPredicate =
    base::RepeatingCallback<bool(const char* category_group_name,
                                 const char* event_name,
                                 ArgumentNameFilterPredicate*)>;

using MetadataFilterPredicate =
    base::RepeatingCallback<bool(const std::string& metadata_name)>;

struct TraceEventHandle {
  uint64_t dummy;
};

class BASE_EXPORT TraceEvent {
 public:
  TraceEvent(PlatformThreadId thread_id,
             TimeTicks timestamp,
             char phase,
             const unsigned char* category_group_enabled,
             const char* name,
             unsigned long long id,
             TraceArguments* args,
             unsigned int flags);
  TraceEvent(const TraceEvent&) = delete;
  TraceEvent& operator=(const TraceEvent&) = delete;
  ~TraceEvent();

  // Allow move operations.
  TraceEvent(TraceEvent&&) noexcept;
  TraceEvent& operator=(TraceEvent&&) noexcept;

  // Reset instance to empty state.
  void Reset();

  TimeTicks timestamp() const { return timestamp_; }
  char phase() const { return phase_; }
  PlatformThreadId thread_id() const { return thread_id_; }
  unsigned long long id() const { return id_; }
  unsigned int flags() const { return flags_; }

  const unsigned char* category_group_enabled() const {
    return category_group_enabled_;
  }

  const char* name() const { return name_; }

  size_t arg_size() const { return args_.size(); }
  unsigned char arg_type(size_t index) const { return args_.types()[index]; }
  const char* arg_name(size_t index) const { return args_.names()[index]; }
  const TraceValue& arg_value(size_t index) const {
    return args_.values()[index];
  }

  ConvertableToTraceFormat* arg_convertible_value(size_t index) {
    return (arg_type(index) == TRACE_VALUE_TYPE_CONVERTABLE)
               ? arg_value(index).as_convertable
               : nullptr;
  }

 private:
  void InitArgs(TraceArguments* args);

  // Note: these are ordered by size (largest first) for optimal packing.
  TimeTicks timestamp_ = TimeTicks();
  // `id_` can be used to store phase-specific data.
  unsigned long long id_ = 0u;
  raw_ptr<const unsigned char> category_group_enabled_ = nullptr;
  const char* name_ = nullptr;
  StringStorage parameter_copy_storage_;
  TraceArguments args_;
  PlatformThreadId thread_id_ = kInvalidThreadId;
  unsigned int flags_ = 0;
  char phase_ = TRACE_EVENT_PHASE_BEGIN;
};

}  // namespace base::trace_event

#endif  // BASE_TRACE_EVENT_TRACE_EVENT_IMPL_H_
