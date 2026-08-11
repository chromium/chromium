// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_TRACE_SESSION_OBSERVER_H_
#define BASE_TRACE_EVENT_TRACE_SESSION_OBSERVER_H_

#include <type_traits>

#include "base/base_export.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/observer_list_threadsafe.h"
#include "base/sequence_checker.h"
#include "base/trace_event/trace_event.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace base::trace_event {

// Perfetto exposes TrackEventSessionObserver to watch for TrackEvent updates.
// In Chrome, TrackEvent is the main mechanism used for tracing, and
// historically there was no distinction. For simplicity, we consider that
// TrackEventSessionObserver is the canonical way to observe trace sessions.
// See perfetto::TrackEventSessionObserver for more details.
using TraceSessionObserver = perfetto::TrackEventSessionObserver;

namespace internal {

template <typename T>
struct TraceObserverTraits {
  static constexpr bool kHasSetup =
      !std::is_same_v<decltype(&T::OnSetup),
                      decltype(&TraceSessionObserver::OnSetup)>;

  static constexpr bool kHasStart =
      !std::is_same_v<decltype(&T::OnStart),
                      decltype(&TraceSessionObserver::OnStart)>;

  static constexpr bool kHasStop =
      !std::is_same_v<decltype(&T::OnStop),
                      decltype(&TraceSessionObserver::OnStop)>;

  static constexpr bool kHasIncremental = !std::is_same_v<
      decltype(&T::WillClearIncrementalState),
      decltype(&TraceSessionObserver::WillClearIncrementalState)>;
};

}  // namespace internal

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

// Proxy class that runs on the main thread.
class MainThreadObserverHolder : public TraceSessionObserver {
 public:
  MainThreadObserverHolder();
  ~MainThreadObserverHolder() override;

  // TraceSessionObserver implementation:
  void OnSetup(const perfetto::DataSourceBase::SetupArgs& args) override;
  void OnStart(const perfetto::DataSourceBase::StartArgs& args) override;
  void OnStop(const perfetto::DataSourceBase::StopArgs& args) override;
  void WillClearIncrementalState(
      const perfetto::DataSourceBase::ClearIncrementalStateArgs& args) override;

  base::ObserverList<TraceSessionObserver>::Unchecked setup_observers_;
  base::ObserverList<TraceSessionObserver>::Unchecked start_observers_;
  base::ObserverList<TraceSessionObserver>::Unchecked stop_observers_;
  base::ObserverList<TraceSessionObserver>::Unchecked
      incremental_state_observers_;
};

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
  template <typename T>
  static void AddObserver(T* observer) {
    static_assert(
        !std::is_same_v<T, TraceSessionObserver>,
        "Do not pass TraceSessionObserver directly. Pass the derived type to "
        "enable compile-time override detection.");
    AddObserverImpl(observer, internal::TraceObserverTraits<T>::kHasSetup,
                    internal::TraceObserverTraits<T>::kHasStart,
                    internal::TraceObserverTraits<T>::kHasStop,
                    internal::TraceObserverTraits<T>::kHasIncremental);
  }

  // Unregister previously registered |observer|.
  template <typename T>
  static void RemoveObserver(T* observer) {
    static_assert(
        !std::is_same_v<T, TraceSessionObserver>,
        "Do not pass TraceSessionObserver directly. Pass the derived type to "
        "enable compile-time override detection.");
    RemoveObserverImpl(observer, internal::TraceObserverTraits<T>::kHasSetup,
                       internal::TraceObserverTraits<T>::kHasStart,
                       internal::TraceObserverTraits<T>::kHasStop,
                       internal::TraceObserverTraits<T>::kHasIncremental);
  }

  // TraceSessionObserver implementation:
  void OnSetup(const perfetto::DataSourceBase::SetupArgs&) override;
  void OnStart(const perfetto::DataSourceBase::StartArgs&) override;
  void OnStop(const perfetto::DataSourceBase::StopArgs&) override;
  void WillClearIncrementalState(
      const perfetto::DataSourceBase::ClearIncrementalStateArgs&) override;

 protected:
  friend class TraceSessionObserverListTest;
  static TraceSessionObserverList& GetInstance();

  static void AddObserverImpl(TraceSessionObserver* observer,
                              bool has_setup,
                              bool has_start,
                              bool has_stop,
                              bool has_incremental);

  static void RemoveObserverImpl(TraceSessionObserver* observer,
                                 bool has_setup,
                                 bool has_start,
                                 bool has_stop,
                                 bool has_incremental);
  using ObserverList =
      base::ObserverListThreadSafe<TraceSessionObserver,
                                   RemoveObserverPolicy::kAddingSequenceOnly>;

  MainThreadObserverHolder main_thread_holder_;

  scoped_refptr<ObserverList> setup_observers_;
  scoped_refptr<ObserverList> start_observers_;
  scoped_refptr<ObserverList> stop_observers_;
  scoped_refptr<ObserverList> incremental_state_observers_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace base::trace_event

#endif  // BASE_TRACE_EVENT_TRACE_SESSION_OBSERVER_H_
