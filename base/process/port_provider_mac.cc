// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/port_provider_mac.h"

#include "base/process/process.h"

namespace base {

PortProvider::PortProvider()
    : observer_list_(MakeRefCounted<ObserverListThreadSafe<Observer>>()) {}
PortProvider::~PortProvider() {}

void PortProvider::AddObserver(Observer* observer) {
  observer_list_->AddObserver(observer);
}

void PortProvider::RemoveObserver(Observer* observer) {
  observer_list_->RemoveObserver(observer);
}

void PortProvider::NotifyObservers(ProcessHandle process) {
  observer_list_->Notify(FROM_HERE, &Observer::OnReceivedTaskPort, process);
}

mach_port_t SelfPortProvider::TaskForPid(base::ProcessHandle process) const {
  DCHECK(base::Process(process).is_current());
  return mach_task_self();
}

}  // namespace base
