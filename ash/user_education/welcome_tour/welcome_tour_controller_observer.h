// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_CONTROLLER_OBSERVER_H_
#define ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_CONTROLLER_OBSERVER_H_

#include "base/observer_list_types.h"

namespace ash {

// An observer for events propagated by the `WelcomeTourController`.
class WelcomeTourControllerObserver : public base::CheckedObserver {
 public:
  // Invoked when the Welcome Tour is started.
  virtual void OnWelcomeTourStarted() {}

  // Invoked when the Welcome Tour is ended, regardless of whether the tour was
  // completed or aborted.
  virtual void OnWelcomeTourEnded() {}
};

}  // namespace ash

#endif  // ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_CONTROLLER_OBSERVER_H_
