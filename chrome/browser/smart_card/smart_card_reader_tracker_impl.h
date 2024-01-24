// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SMART_CARD_SMART_CARD_READER_TRACKER_IMPL_H_
#define CHROME_BROWSER_SMART_CARD_SMART_CARD_READER_TRACKER_IMPL_H_

#include <memory>
#include <string>

#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
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
  // State machine graph:
  // https://drive.google.com/file/d/1UbthxgzsK-fu8UKZ7xTgRWN-oi_HIN_I/view
  class State;
  class WaitContext;
  class WaitInitialReaderStatus;
  class Tracking;
  class Uninitialized;
  class WaitReadersList;

  ////
  // `State` code can call all methods and access all member variables of `this`
  // except for the ones at the very bottom (see corresponding comment there).

  void ChangeState(std::unique_ptr<State> next_state);
  bool CanTrack() const;
  void AddReader(const device::mojom::SmartCardReaderStateOut& state_out);
  void AddOrUpdateReader(
      const device::mojom::SmartCardReaderStateOut& state_out);
  void RemoveReader(const device::mojom::SmartCardReaderStateOut& state_out);

  void GetReadersFromCache(StartCallback callback);
  void UpdateCache(const std::vector<device::mojom::SmartCardReaderStateOutPtr>&
                       reader_states);

  // Fulfill all requests in `pending_get_readers_requests_` with information
  // from the cache.
  void FulfillRequests();
  // Fails all requests in `pending_get_readers_requests_` with the given error.
  void FailRequests(device::mojom::SmartCardError error);

  static ReaderInfo ReaderInfoFromMojoStateOut(
      const device::mojom::SmartCardReaderStateOut& state_out);

  static bool CopyStateFlagsIfChanged(
      ReaderInfo& info,
      const device::mojom::SmartCardReaderStateFlags& state_flags);

  // Returns whether any change was made.
  static bool UpdateInfoIfChanged(
      ReaderInfo& info,
      const device::mojom::SmartCardReaderStateOut& state_out);

  static device::mojom::SmartCardReaderStateFlagsPtr CurrentStateFlagsFromInfo(
      const ReaderInfo& info);

  ObserverList observer_list_;
  mojo::Remote<device::mojom::SmartCardContextFactory> context_factory_;

  base::queue<StartCallback> pending_get_readers_requests_;

  // The known state of smart card readers.
  std::map<std::string, ReaderInfo> readers_;

  ////
  // member variables below are not directly accessed by `State` code.

  // Current state.
  std::unique_ptr<State> state_;

  base::WeakPtrFactory<SmartCardReaderTrackerImpl> weak_factory_{this};
};

#endif  // CHROME_BROWSER_SMART_CARD_SMART_CARD_READER_TRACKER_IMPL_H_
