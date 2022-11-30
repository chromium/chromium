// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_KEYBOARD_ARC_ARC_INPUT_METHOD_BOUNDS_TRACKER_H_
#define ASH_PUBLIC_CPP_KEYBOARD_ARC_ARC_INPUT_METHOD_BOUNDS_TRACKER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/observer_list.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

// This interface keeps tracking the bounds change events of ARC IMEs
// and notifies registered observers of bounds changes.
class ASH_PUBLIC_EXPORT ArcInputMethodBoundsTracker {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;
    virtual void OnArcInputMethodBoundsChanged(const gfx::Rect& bounds) = 0;
  };

  // Gets the global ArcInputMethodBoundsTracker instance.
  static ArcInputMethodBoundsTracker* Get();

  ArcInputMethodBoundsTracker();
  virtual ~ArcInputMethodBoundsTracker();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  void NotifyArcInputMethodBoundsChanged(const gfx::Rect& bounds);

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_KEYBOARD_ARC_ARC_INPUT_METHOD_BOUNDS_TRACKER_H_
