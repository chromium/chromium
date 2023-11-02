// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_HOME_LAUNCHER_ANIMATION_INFO_H_
#define ASH_APP_LIST_HOME_LAUNCHER_ANIMATION_INFO_H_

namespace ash {

// The reason a home launcher animation was triggered.
enum class HomeLauncherAnimationTrigger {
  // Launcher animation is triggered by pressing the AppList button.
  kLauncherButton,

  // Launcher animation is triggered by window activation.
  kHideForWindow,

  // Launcher animation is triggered by entering/exiting overview mode where
  // overview UI fades in/out.
  kOverviewModeFade
};

// Information used to configure animation metrics reporter when animating the
// home launcher.
struct HomeLauncherAnimationInfo {
  HomeLauncherAnimationInfo(HomeLauncherAnimationTrigger trigger, bool showing)
      : trigger(trigger), showing(showing) {}
  ~HomeLauncherAnimationInfo() = default;

  // The animation trigger.
  const HomeLauncherAnimationTrigger trigger;

  // Whether the home screen will be shown at the end of the animation.
  const bool showing;
};

}  // namespace ash

#endif  // ASH_APP_LIST_HOME_LAUNCHER_ANIMATION_INFO_H_
