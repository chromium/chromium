// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/smart_card/smart_card_reader_tracker.h"

SmartCardReaderTracker::ReaderInfo::ReaderInfo() = default;
SmartCardReaderTracker::ReaderInfo::ReaderInfo(ReaderInfo&& other) = default;
SmartCardReaderTracker::ReaderInfo::ReaderInfo(const ReaderInfo&) = default;
SmartCardReaderTracker::ReaderInfo::~ReaderInfo() = default;

SmartCardReaderTracker::ReaderInfo&
SmartCardReaderTracker::ReaderInfo::operator=(
    const SmartCardReaderTracker::ReaderInfo& other) = default;

bool SmartCardReaderTracker::ReaderInfo::operator==(const ReaderInfo& b) const =
    default;

SmartCardReaderTracker::ObserverList::ObserverList() = default;
SmartCardReaderTracker::ObserverList::~ObserverList() = default;

void SmartCardReaderTracker::ObserverList::AddObserverIfMissing(
    Observer* observer) {
  if (!observers_.HasObserver(observer)) {
    observers_.AddObserver(observer);
  }
}

void SmartCardReaderTracker::ObserverList::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SmartCardReaderTracker::ObserverList::NotifyReaderChanged(
    const ReaderInfo& reader_info) {
  for (Observer& obs : observers_) {
    obs.OnReaderChanged(reader_info);
  }
}

void SmartCardReaderTracker::ObserverList::NotifyReaderRemoved(
    const std::string& reader_name) {
  for (Observer& obs : observers_) {
    obs.OnReaderRemoved(reader_name);
  }
}

void SmartCardReaderTracker::ObserverList::NotifyError(
    device::mojom::SmartCardError error) {
  for (Observer& obs : observers_) {
    obs.OnError(error);
  }
}
