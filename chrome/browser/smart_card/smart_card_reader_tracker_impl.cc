// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/smart_card/smart_card_reader_tracker_impl.h"

SmartCardReaderTrackerImpl::SmartCardReaderTrackerImpl(
    mojo::PendingRemote<device::mojom::SmartCardContextFactory> context_factory)
    : context_factory_(std::move(context_factory)) {}

SmartCardReaderTrackerImpl::~SmartCardReaderTrackerImpl() = default;

void SmartCardReaderTrackerImpl::Start(Observer* observer,
                                       StartCallback callback) {
  observer_list_.AddObserverIfMissing(observer);
  // TODO(crbug.com/1464851): Implement.
  std::move(callback).Run(std::nullopt);
}

void SmartCardReaderTrackerImpl::Stop(Observer* observer) {
  observer_list_.RemoveObserver(observer);
  // TODO(crbug.com/1464851): Implement.
}
