// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_CHROMEVOX_HINT_CHROMEVOX_HINT_DETECTOR_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_CHROMEVOX_HINT_CHROMEVOX_HINT_DETECTOR_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace base {
class TickClock;
}  // namespace base

namespace ash {

class IdleDetector;

// Helper for ChromeVox hint idle detection.
class ChromeVoxHintDetector {
 public:
  // Interface for notification that the device stayed idle long enough to
  // trigger the ChromeVox hint.
  class Observer {
   public:
    virtual void OnShouldGiveChromeVoxHint() = 0;
    virtual ~Observer() = default;
  };

  explicit ChromeVoxHintDetector(const base::TickClock* clock,
                                 Observer* observer);
  ChromeVoxHintDetector(const ChromeVoxHintDetector&) = delete;
  ChromeVoxHintDetector& operator=(const ChromeVoxHintDetector&) = delete;
  virtual ~ChromeVoxHintDetector();

 private:
  friend class WelcomeScreenChromeVoxHintTest;

  void StartIdleDetection();
  void OnIdle();

  bool chromevox_hint_given_ = false;

  raw_ptr<const base::TickClock> tick_clock_;

  raw_ptr<Observer> observer_;

  std::unique_ptr<IdleDetector> idle_detector_;

  base::WeakPtrFactory<ChromeVoxHintDetector> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_CHROMEVOX_HINT_CHROMEVOX_HINT_DETECTOR_H_
