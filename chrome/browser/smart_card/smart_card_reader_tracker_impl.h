// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SMART_CARD_SMART_CARD_READER_TRACKER_IMPL_H_
#define CHROME_BROWSER_SMART_CARD_SMART_CARD_READER_TRACKER_IMPL_H_

#include "chrome/browser/smart_card/smart_card_reader_tracker.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/smart_card.mojom.h"

namespace base {
class TimeDelta;
}

class SmartCardReaderTrackerImpl : public SmartCardReaderTracker {
 public:
  // Minimum time between consecutive Start() calls necessary
  // to trigger a restart.
  static const base::TimeDelta kMinRefreshInterval;

  explicit SmartCardReaderTrackerImpl(
      mojo::PendingRemote<device::mojom::SmartCardContextFactory>);

  ~SmartCardReaderTrackerImpl() override;

  // `SmartCardReaderTracker` overrides:
  void Start(Observer* observer, StartCallback) override;
  void Stop(Observer* observer) override;

 private:
  ObserverList observer_list_;
  mojo::Remote<device::mojom::SmartCardContextFactory> context_factory_;
};

#endif  // CHROME_BROWSER_SMART_CARD_SMART_CARD_READER_TRACKER_IMPL_H_
