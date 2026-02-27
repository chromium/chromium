// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_TRACE_LOG_H_
#define BASE_TRACE_EVENT_TRACE_LOG_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/base_export.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/process/process_handle.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/trace_event/trace_config.h"
#include "base/trace_event/trace_event_impl.h"
#include "build/build_config.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"  // IWYU pragma: keep
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"

namespace perfetto {
namespace trace_processor {
class TraceProcessorStorage;
}  // namespace trace_processor
}  // namespace perfetto

namespace base {
class RefCountedString;

namespace trace_event {

class JsonStringOutputWriter;

class BASE_EXPORT TraceLog {
 public:
  static TraceLog* GetInstance();

  TraceLog(const TraceLog&) = delete;
  TraceLog& operator=(const TraceLog&) = delete;

  // See TraceConfig comments for details on how to control which categories
  // will be traced.
  void SetEnabled(const TraceConfig& trace_config);

  // Enable tracing using a customized Perfetto trace config. This allows, for
  // example, enabling additional data sources and enabling protobuf output
  // instead of the legacy JSON trace format.
  void SetEnabled(const TraceConfig& trace_config,
                  const perfetto::TraceConfig& perfetto_config);

  // Disables tracing for all categories.
  void SetDisabled();

  // Flush all collected events to the given output callback. The callback will
  // be called one or more times either synchronously or asynchronously from
  // the current thread with IPC-bite-size chunks. The string format is
  // undefined. Use TraceResultBuffer to convert one or more trace strings to
  // JSON. The callback can be null if the caller doesn't want any data.
  // Due to the implementation of thread-local buffers, flush can't be
  // done when tracing is enabled. If called when tracing is enabled, the
  // callback will be called directly with (empty_string, false) to indicate
  // the end of this unsuccessful flush. Flush does the serialization
  // on the same thread if the caller doesn't set use_worker_thread explicitly.
  using OutputCallback =
      base::RepeatingCallback<void(const scoped_refptr<base::RefCountedString>&,
                                   bool has_more_events)>;
  void Flush(const OutputCallback& cb, bool use_worker_thread = false);

  // Cancels tracing and discards collected data.
  void CancelTracing(const OutputCallback& cb);

  ProcessId process_id() const { return process_id_; }

  // Exposed for unittesting:
  // Allows clearing up our singleton instance.
  static void ResetForTesting();

  void SetProcessID(ProcessId process_id);

 private:
  friend class base::NoDestructor<TraceLog>;

  TraceLog();
  ~TraceLog();

  void SetEnabledImpl(const TraceConfig& trace_config,
                      const perfetto::TraceConfig& perfetto_config);

  void SetDisabledWhileLocked() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  void FlushInternal(const OutputCallback& cb,
                     bool use_worker_thread,
                     bool discard_events);

  void OnTraceData(const char* data, size_t size, bool has_more);

  // This lock protects TraceLog member accesses (except for members protected
  // by thread_info_lock_) from arbitrary threads.
  mutable Lock lock_;

  ProcessId process_id_;

  std::unique_ptr<perfetto::TracingSession> tracing_session_;
  perfetto::TraceConfig perfetto_config_;
#if BUILDFLAG(USE_PERFETTO_TRACE_PROCESSOR)
  std::unique_ptr<perfetto::trace_processor::TraceProcessorStorage>
      trace_processor_;
  std::unique_ptr<JsonStringOutputWriter> json_output_writer_;
  OutputCallback proto_output_callback_;
#endif  // BUILDFLAG(USE_PERFETTO_TRACE_PROCESSOR)
};

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_TRACE_LOG_H_
