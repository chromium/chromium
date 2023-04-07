// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_WELCOME_TOUR_MOCK_WELCOME_TOUR_CONTROLLER_OBSERVER_H_
#define ASH_USER_EDUCATION_WELCOME_TOUR_MOCK_WELCOME_TOUR_CONTROLLER_OBSERVER_H_

#include "ash/ash_export.h"
#include "ash/user_education/welcome_tour/welcome_tour_controller_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

// A mock implementation of an observer for events propagated by the
// `WelcomeTourController`.
class ASH_EXPORT MockWelcomeTourControllerObserver
    : public WelcomeTourControllerObserver {
 public:
  MockWelcomeTourControllerObserver();
  MockWelcomeTourControllerObserver(const MockWelcomeTourControllerObserver&) =
      delete;
  MockWelcomeTourControllerObserver& operator=(
      const MockWelcomeTourControllerObserver&) = delete;
  ~MockWelcomeTourControllerObserver() override;

  // WelcomeTourControllerObserver:
  MOCK_METHOD(void, OnWelcomeTourStarted, (), (override));
  MOCK_METHOD(void, OnWelcomeTourEnded, (), (override));
};

}  // namespace ash

#endif  // ASH_USER_EDUCATION_WELCOME_TOUR_MOCK_WELCOME_TOUR_CONTROLLER_OBSERVER_H_
