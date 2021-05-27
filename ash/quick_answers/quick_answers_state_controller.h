// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_ANSWERS_QUICK_ANSWERS_STATE_CONTROLLER_H_
#define ASH_QUICK_ANSWERS_QUICK_ANSWERS_STATE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/quick_answers/quick_answers_state.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/macros.h"

namespace ash {

// Provide access of Assistant related prefs and states to the clients.
class ASH_EXPORT QuickAnswersStateController : public SessionObserver {
 public:
  QuickAnswersStateController();

  QuickAnswersStateController(const QuickAnswersStateController&) = delete;
  QuickAnswersStateController& operator=(const QuickAnswersStateController&) =
      delete;

  ~QuickAnswersStateController() override;

 private:
  // SessionObserver:
  void OnFirstSessionStarted() override;

  QuickAnswersState state_;

  ScopedSessionObserver session_observer_;
};

}  // namespace ash

#endif  // ASH_QUICK_ANSWERS_QUICK_ANSWERS_STATE_CONTROLLER_H_
