// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/trace_session_observer.h"

#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"

namespace base::trace_event {
namespace {

class StopArgsImpl : public perfetto::DataSourceBase::StopArgs {
 public:
  explicit StopArgsImpl(const perfetto::DataSourceBase::StopArgs& args)
      : StopArgs(args) {}

  std::function<void()> HandleStopAsynchronously() const override {  // nocheck
    // HandleStopAsynchronously not supported.
    NOTREACHED();
  }
};

bool IsEnabledExludingOnStopInstance(
    uint32_t instances,
    const perfetto::DataSourceBase::StopArgs& args) {
  return instances &
         ~(static_cast<uint32_t>(1) << args.internal_instance_index);
}

}  // namespace

bool IsEnabledOnStop(const perfetto::DataSourceBase::StopArgs& args) {
  bool enabled = true;
  base::TrackEvent::CallIfEnabled([&](uint32_t instances) {
    enabled = IsEnabledExludingOnStopInstance(instances, args);
  });
  return enabled;
}

bool IsCategoryEnabledOnStop(size_t category_index,
                             const perfetto::DataSourceBase::StopArgs& args) {
  bool enabled = true;
  base::TrackEvent::CallIfCategoryEnabled(
      category_index, [&](uint32_t instances) {
        enabled = IsEnabledExludingOnStopInstance(instances, args);
      });
  return enabled;
}

MainThreadObserverHolder::MainThreadObserverHolder() = default;
MainThreadObserverHolder::~MainThreadObserverHolder() = default;

void MainThreadObserverHolder::OnSetup(
    const perfetto::DataSourceBase::SetupArgs& args) {
  setup_observers_.Notify(&TraceSessionObserver::OnSetup, args);
}

void MainThreadObserverHolder::OnStart(
    const perfetto::DataSourceBase::StartArgs& args) {
  start_observers_.Notify(&TraceSessionObserver::OnStart, args);
}

void MainThreadObserverHolder::OnStop(
    const perfetto::DataSourceBase::StopArgs& args) {
  stop_observers_.Notify(&TraceSessionObserver::OnStop, args);
}

void MainThreadObserverHolder::WillClearIncrementalState(
    const perfetto::DataSourceBase::ClearIncrementalStateArgs& args) {
  incremental_state_observers_.Notify(
      &TraceSessionObserver::WillClearIncrementalState, args);
}

// static
void TraceSessionObserverList::Initialize() {
  GetInstance();
}

// static
TraceSessionObserverList& TraceSessionObserverList::GetInstance() {
  static NoDestructor<TraceSessionObserverList> instance;
  return *instance;
}

TraceSessionObserverList::TraceSessionObserverList()
    : setup_observers_(base::MakeRefCounted<ObserverList>()),
      start_observers_(base::MakeRefCounted<ObserverList>()),
      stop_observers_(base::MakeRefCounted<ObserverList>()),
      incremental_state_observers_(base::MakeRefCounted<ObserverList>()) {
  if (base::SingleThreadTaskRunner::HasMainThreadDefault()) {
    main_thread_task_runner_ =
        base::SingleThreadTaskRunner::GetMainThreadDefault();
  }
  base::TrackEvent::AddSessionObserver(this);
}

TraceSessionObserverList::~TraceSessionObserverList() {
  base::TrackEvent::RemoveSessionObserver(this);
}

void TraceSessionObserverList::AddObserverImpl(TraceSessionObserver* observer,
                                               bool has_setup,
                                               bool has_start,
                                               bool has_stop,
                                               bool has_incremental) {
  TraceSessionObserverList& instance = GetInstance();
  bool is_main_thread =
      instance.main_thread_task_runner_ &&
      instance.main_thread_task_runner_->RunsTasksInCurrentSequence();

  MainThreadObserverHolder* holder =
      is_main_thread ? &instance.main_thread_holder_ : nullptr;

  if (has_setup) {
    if (is_main_thread) {
      if (holder->setup_observers_.empty()) {
        instance.setup_observers_->AddObserver(holder);
      }
      holder->setup_observers_.AddObserver(observer);
    } else {
      instance.setup_observers_->AddObserver(observer);
    }
  }
  if (has_start) {
    if (is_main_thread) {
      if (holder->start_observers_.empty()) {
        instance.start_observers_->AddObserver(holder);
      }
      holder->start_observers_.AddObserver(observer);
    } else {
      instance.start_observers_->AddObserver(observer);
    }
  }
  if (has_stop) {
    if (is_main_thread) {
      if (holder->stop_observers_.empty()) {
        instance.stop_observers_->AddObserver(holder);
      }
      holder->stop_observers_.AddObserver(observer);
    } else {
      instance.stop_observers_->AddObserver(observer);
    }
  }
  if (has_incremental) {
    if (is_main_thread) {
      if (holder->incremental_state_observers_.empty()) {
        instance.incremental_state_observers_->AddObserver(holder);
      }
      holder->incremental_state_observers_.AddObserver(observer);
    } else {
      instance.incremental_state_observers_->AddObserver(observer);
    }
  }
}

void TraceSessionObserverList::RemoveObserverImpl(
    TraceSessionObserver* observer,
    bool has_setup,
    bool has_start,
    bool has_stop,
    bool has_incremental) {
  TraceSessionObserverList& instance = GetInstance();
  bool is_main_thread =
      instance.main_thread_task_runner_ &&
      instance.main_thread_task_runner_->RunsTasksInCurrentSequence();

  MainThreadObserverHolder* holder =
      is_main_thread ? &instance.main_thread_holder_ : nullptr;

  if (has_setup) {
    if (is_main_thread) {
      holder->setup_observers_.RemoveObserver(observer);
      if (holder->setup_observers_.empty()) {
        instance.setup_observers_->RemoveObserver(holder);
      }
    } else {
      instance.setup_observers_->RemoveObserver(observer);
    }
  }
  if (has_start) {
    if (is_main_thread) {
      holder->start_observers_.RemoveObserver(observer);
      if (holder->start_observers_.empty()) {
        instance.start_observers_->RemoveObserver(holder);
      }
    } else {
      instance.start_observers_->RemoveObserver(observer);
    }
  }
  if (has_stop) {
    if (is_main_thread) {
      holder->stop_observers_.RemoveObserver(observer);
      if (holder->stop_observers_.empty()) {
        instance.stop_observers_->RemoveObserver(holder);
      }
    } else {
      instance.stop_observers_->RemoveObserver(observer);
    }
  }
  if (has_incremental) {
    if (is_main_thread) {
      holder->incremental_state_observers_.RemoveObserver(observer);
      if (holder->incremental_state_observers_.empty()) {
        instance.incremental_state_observers_->RemoveObserver(holder);
      }
    } else {
      instance.incremental_state_observers_->RemoveObserver(observer);
    }
  }
}

void TraceSessionObserverList::OnSetup(
    const perfetto::DataSourceBase::SetupArgs& args) {
  setup_observers_->Notify(FROM_HERE, &TraceSessionObserver::OnSetup, args);
}

void TraceSessionObserverList::OnStart(
    const perfetto::DataSourceBase::StartArgs& args) {
  start_observers_->Notify(FROM_HERE, &TraceSessionObserver::OnStart, args);
}

void TraceSessionObserverList::OnStop(
    const perfetto::DataSourceBase::StopArgs& args) {
  stop_observers_->Notify(FROM_HERE, &TraceSessionObserver::OnStop,
                          StopArgsImpl{args});
}

void TraceSessionObserverList::WillClearIncrementalState(
    const perfetto::DataSourceBase::ClearIncrementalStateArgs& args) {
  incremental_state_observers_->Notify(
      FROM_HERE, &TraceSessionObserver::WillClearIncrementalState, args);
}

}  // namespace base::trace_event
