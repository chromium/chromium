// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/trace_session_observer.h"

#include "base/no_destructor.h"
#include "base/notreached.h"

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
    : observers_(base::MakeRefCounted<ObserverList>()) {
  base::TrackEvent::AddSessionObserver(this);
}

TraceSessionObserverList::~TraceSessionObserverList() {
  base::TrackEvent::RemoveSessionObserver(this);
}

void TraceSessionObserverList::AddObserver(
    perfetto::TrackEventSessionObserver* observer) {
  GetInstance().observers_->AddObserver(observer);
}

void TraceSessionObserverList::RemoveObserver(
    perfetto::TrackEventSessionObserver* observer) {
  GetInstance().observers_->RemoveObserver(observer);
}

void TraceSessionObserverList::OnSetup(
    const perfetto::DataSourceBase::SetupArgs& args) {
  observers_->Notify(FROM_HERE, &perfetto::TrackEventSessionObserver::OnSetup,
                     args);
}

void TraceSessionObserverList::OnStart(
    const perfetto::DataSourceBase::StartArgs& args) {
  observers_->Notify(FROM_HERE, &perfetto::TrackEventSessionObserver::OnStart,
                     args);
}

void TraceSessionObserverList::OnStop(
    const perfetto::DataSourceBase::StopArgs& args) {
  observers_->Notify(FROM_HERE, &perfetto::TrackEventSessionObserver::OnStop,
                     StopArgsImpl{args});
}

void TraceSessionObserverList::WillClearIncrementalState(
    const perfetto::DataSourceBase::ClearIncrementalStateArgs& args) {
  observers_->Notify(
      FROM_HERE,
      &perfetto::TrackEventSessionObserver::WillClearIncrementalState, args);
}

}  // namespace base::trace_event
