// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_TRACE_SESSION_OBSERVER_H_
#define BASE_TRACE_EVENT_TRACE_SESSION_OBSERVER_H_

#include "base/base_export.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list_threadsafe.h"
#include "base/sequence_checker.h"
#include "base/trace_event/trace_event.h"

namespace base::trace_event {

// Perfetto exposes TrackEventSessionObserver to watch for TrackEvent updates.
// In Chrome, TrackEvent is the main mechanism used for tracing, and
// historically there was no distinction. For simplicity, we consider that
// TrackEventSessionObserver is the canonical way to observe trace sessions.
// See perfetto::TrackEventSessionObserver for more details.
using TraceSessionObserver = perfetto::TrackEventSessionObserver;

// Returns true if any tracing instance is enabled, ignoring a given session
// that's being stopped. This is useful to call in TraceSessionObserver::OnStop,
// to test if any other instance will still be enabled.
BASE_EXPORT bool IsEnabledOnStop(
    const perfetto::DataSourceBase::StopArgs& args);

// Same as above, for a specific tracing category.
// IsCategoryEnabledOnStop(PERFETTO_GET_CATEGORY_INDEX("my_category"), args);
BASE_EXPORT bool IsCategoryEnabledOnStop(
    size_t category_index,
    const perfetto::DataSourceBase::StopArgs& args);

// A thread-safe list of TraceSessionObserver. Observers are always notified on
// the sequence from which they were registered. If you don't need sequence
// afine observer, use base::TraceEvent::AddSessionObserver directly.
class BASE_EXPORT TraceSessionObserverList : public TraceSessionObserver {
 public:
  static void Initialize();

  TraceSessionObserverList();
  TraceSessionObserverList(const TraceSessionObserverList&) = delete;
  TraceSessionObserverList& operator=(const TraceSessionObserverList&) = delete;
  ~TraceSessionObserverList() override;

  // Register |observer| to get tracing notifications.
  static void AddObserver(TraceSessionObserver* observer);
  // Unregister previously registered |observer|.
  static void RemoveObserver(TraceSessionObserver* observer);

  // TraceSessionObserver implementation:
  void OnSetup(const perfetto::DataSourceBase::SetupArgs&) override;
  void OnStart(const perfetto::DataSourceBase::StartArgs&) override;
  void OnStop(const perfetto::DataSourceBase::StopArgs&) override;
  void WillClearIncrementalState(
      const perfetto::DataSourceBase::ClearIncrementalStateArgs&) override;

 protected:
  static TraceSessionObserverList& GetInstance();

  using ObserverList =
      base::ObserverListThreadSafe<TrackEventSessionObserver,
                                   RemoveObserverPolicy::kAddingSequenceOnly>;
  scoped_refptr<ObserverList> observers_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace base::trace_event

#endif  // BASE_TRACE_EVENT_TRACE_SESSION_OBSERVER_H_
