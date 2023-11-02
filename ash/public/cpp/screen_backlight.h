// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SCREEN_BACKLIGHT_H_
#define ASH_PUBLIC_CPP_SCREEN_BACKLIGHT_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/scoped_singleton_resetter_for_test.h"
#include "ash/public/cpp/screen_backlight_observer.h"
#include "ash/public/cpp/screen_backlight_type.h"

namespace ash {

// An interface implemented by Ash that allows Chrome to be informed of
// screen backlight events.
class ASH_PUBLIC_EXPORT ScreenBacklight {
 public:
  using ScopedResetterForTest = ScopedSingletonResetterForTest<ScreenBacklight>;

  // Returns the singleton instance.
  static ScreenBacklight* Get();

  virtual void AddObserver(ScreenBacklightObserver* observer) = 0;
  virtual void RemoveObserver(ScreenBacklightObserver* observer) = 0;

  // Returns current system screen backlight state.
  virtual ScreenBacklightState GetScreenBacklightState() const = 0;

 protected:
  ScreenBacklight();
  virtual ~ScreenBacklight();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SCREEN_BACKLIGHT_H_
