// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_STATE_CONTROLLER_H_
#define ASH_ASSISTANT_ASSISTANT_STATE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/session/session_observer.h"

namespace ash {

// Provide access of Assistant related prefs and states to the clients.
class ASH_EXPORT AssistantStateController : public AssistantState,
                                            public SessionObserver {
 public:
  AssistantStateController();

  AssistantStateController(const AssistantStateController&) = delete;
  AssistantStateController& operator=(const AssistantStateController&) = delete;

  ~AssistantStateController() override;

 private:
  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  ScopedSessionObserver session_observer_;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_STATE_CONTROLLER_H_
