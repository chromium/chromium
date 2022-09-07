// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_KEY_HOLD_DETECTOR_H_
#define ASH_ACCELERATORS_KEY_HOLD_DETECTOR_H_

#include <memory>

#include "ash/ash_export.h"
#include "ui/events/event_handler.h"

namespace ui {
class KeyEvent;
}

namespace ash {

// This class is used to implement action when a user press and hold the key.
// When a user just pressed and released a key, normal pressed event gets
// generated upon release.
class ASH_EXPORT KeyHoldDetector : public ui::EventHandler {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // If this return false, the event handler does not process
    // the event at all.
    virtual bool ShouldProcessEvent(const ui::KeyEvent* event) const = 0;

    // This should return true if the event should start the state transition.
    virtual bool IsStartEvent(const ui::KeyEvent* event) const = 0;

    // Whether to stop event propagation after processing.
    virtual bool ShouldStopEventPropagation() const = 0;

    // Called when the key is held.
    virtual void OnKeyHold(const ui::KeyEvent* event) = 0;

    // Called when the key is release after hold.
    virtual void OnKeyUnhold(const ui::KeyEvent* event) = 0;
  };

  explicit KeyHoldDetector(std::unique_ptr<Delegate> delegate);
  KeyHoldDetector(const KeyHoldDetector&) = delete;
  KeyHoldDetector& operator=(const KeyHoldDetector&) = delete;
  ~KeyHoldDetector() override;

  // ui::EventHandler overrides:
  void OnKeyEvent(ui::KeyEvent* key_event) override;

 private:
  // A state to keep track of one click and click and hold operation.
  //
  // One click:
  // INITIAL --(first press)--> PRESSED --(release)--> INITIAL[SEND PRESS]
  //
  // Click and hold:
  // INITIAL --(first press)--> PRESSED --(press)-->
  //   HOLD[OnKeyHold] --(press)--> HOLD[OnKeyHold] --(release)-->
  //   INITIAL[OnKeyUnhold]
  enum State { INITIAL, PRESSED, HOLD };

  State state_;
  std::unique_ptr<Delegate> delegate_;
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_KEY_HOLD_DETECTOR_H_
