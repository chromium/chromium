// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SMART_CARD_SMART_CARD_READER_TRACKER_H_
#define CHROME_BROWSER_SMART_CARD_SMART_CARD_READER_TRACKER_H_

#include <optional>

#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "services/device/public/mojom/smart_card.mojom.h"

// Keeps track of the current list of readers and their states.
// Notifies about changes via an observer pattern.
class SmartCardReaderTracker : public KeyedService {
 public:
  struct ReaderInfo {
    ReaderInfo();
    ReaderInfo(ReaderInfo&& other);
    ReaderInfo(const ReaderInfo&);
    ~ReaderInfo();

    ReaderInfo& operator=(const ReaderInfo& other);
    bool operator==(const ReaderInfo& b) const;

    std::string name;

    // A subset of device::mojom::SmartCardReaderStateFlags
    bool unavailable = false;
    bool empty = false;
    bool present = false;
    bool exclusive = false;
    bool inuse = false;
    bool mute = false;
    bool unpowered = false;

    // Number of card insertion and removal events that happened in this reader.
    // Will always be zero if not supported by the platform.
    uint16_t event_count = 0;

    std::vector<uint8_t> answer_to_reset;
  };

  // Observer class for changes to smart card readers.
  //
  // Note that there's no OnReaderAdded() for two reasons:
  //   1 - The browser code does not need it.
  //   2 - The underlying PC/SC feature needed to implement this is not present
  //       in all platforms (it's missing on MacOS).
  class Observer : public base::CheckedObserver {
   public:
    // Called when a smart card reader is removed from the system.
    virtual void OnReaderRemoved(const std::string& reader_name) {}

    // Called when the attributes (state and/or atr) of a smart card reader
    // changes.
    virtual void OnReaderChanged(const ReaderInfo& reader_info) {}

    // Called when a error preventing the monitoring of reader changes occurs.
    // Can be retried with a new `Start` call.
    virtual void OnError(device::mojom::SmartCardError error) {}
  };

  class ObserverList {
   public:
    ObserverList();
    ObserverList(const ObserverList&) = delete;
    ObserverList& operator=(const ObserverList&) = delete;
    ~ObserverList();

    bool empty() const { return observers_.empty(); }

    void AddObserverIfMissing(Observer* observer);
    void RemoveObserver(Observer* observer);

    void NotifyReaderChanged(const ReaderInfo& reader_info);
    void NotifyReaderRemoved(const std::string& reader_name);
    void NotifyError(device::mojom::SmartCardError error);

   private:
    base::ObserverList<Observer> observers_;
  };

  // The parameter is a list of readers currently available.
  //
  // If a PC/SC error occurred, there will be no list. Ie, the optional will
  // have no value.
  //
  // If the list is empty, tracking will also have stopped as there are no
  // readers to track.
  using StartCallback =
      base::OnceCallback<void(std::optional<std::vector<ReaderInfo>>)>;

  SmartCardReaderTracker() = default;
  ~SmartCardReaderTracker() override = default;

  // Returns the list of currently available smart card readers and (re)starts
  // tracking them for changes or removals.
  //
  // It will stop tracking once there are no more observers, upon the first
  // error encountered or if there are no readers in the system.
  virtual void Start(Observer* observer, StartCallback) = 0;

  // Removes an observer and stops tracking smart card reader
  // changes/additions/removals if there are no other observers left
  virtual void Stop(Observer* observer) = 0;
};

#endif  // CHROME_BROWSER_SMART_CARD_SMART_CARD_READER_TRACKER_H_
