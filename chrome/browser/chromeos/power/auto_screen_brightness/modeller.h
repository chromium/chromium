// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_MODELLER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_MODELLER_H_

#include "base/observer_list_types.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/monotone_cubic_spline.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

struct Model;

// Interface to on-device adaptive model.
class Modeller {
 public:
  // Modeller must outlive its observers.
  class Observer : public base::CheckedObserver {
   public:
    Observer() = default;
    ~Observer() override = default;

    // Called when a new curve (|brightness_curve|) is trained.
    virtual void OnModelTrained(
        const MonotoneCubicSpline& brightness_curve) = 0;

    // Called when the model is initialized. If model is disabled, both
    // |global_curve| and |personal_curve| will be nullopt. If there is only a
    // global curve, then |personal_curve| will be nullopt.
    virtual void OnModelInitialized(const Model& model) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  virtual ~Modeller() = default;

  // Adds or removes an observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_MODELLER_H_
