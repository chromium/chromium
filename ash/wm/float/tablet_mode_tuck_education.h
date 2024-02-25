// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_FLOAT_TABLET_MODE_TUCK_EDUCATION_H_
#define ASH_WM_FLOAT_TABLET_MODE_TUCK_EDUCATION_H_

#include "ash/ash_export.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/clock.h"
#include "components/prefs/pref_registry_simple.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace ash {

// This class is responsible for educating users about the tuck gesture when
// a window is floated in tablet mode. It shows a nudge with text indicating the
// gesture to tuck, and bounces the floated window twice.
class ASH_EXPORT TabletModeTuckEducation : public aura::WindowObserver {
 public:
  explicit TabletModeTuckEducation(aura::Window* floated_window);
  TabletModeTuckEducation(const TabletModeTuckEducation&) = delete;
  TabletModeTuckEducation& operator=(const TabletModeTuckEducation&) = delete;
  ~TabletModeTuckEducation() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Returns false if the tuck education has been shown 3 or more times or
  // within the last 24 hours, and true otherwise.
  static bool CanActivateTuckEducation();

  // Sets the nudge count to its maximum value so it doesn't appear anymore.
  static void OnWindowTucked();

  // aura::WindowObserver:
  void OnWindowTransformed(aura::Window* window,
                           ui::PropertyChangeReason reason) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(TabletWindowFloatTest, EducationPreferences);

  // Activates the tuck education nudge and bounce animations for
  // `floated_window` if it has not already been shown 3 times or within the
  // last 24 hours.
  void ActivateTuckEducation();

  // Dismisses the nudge if it is still active, and cleans up all related
  // pointers.
  void DismissNudge();

  // Used to control the clock in a test setting.
  static void SetOverrideClockForTesting(base::Clock* test_clock);

  // The widget that contains the `RoundedLabel`.
  views::UniqueWidgetPtr nudge_widget_;

  // The floated window that `nudge_widget_` is a child of. Guaranteed to be
  // alive for the lifetime of `this` since the owner of `this` observes
  // `OnWindowDestroying()`.
  raw_ptr<aura::Window> window_ = nullptr;

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};

  // Chrome's compiler toolchain enforces that any `WeakPtrFactory`
  // fields are declared last, to avoid destruction ordering issues.
  base::WeakPtrFactory<TabletModeTuckEducation> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_FLOAT_TABLET_MODE_TUCK_EDUCATION_H_
