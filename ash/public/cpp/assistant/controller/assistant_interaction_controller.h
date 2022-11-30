// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_INTERACTION_CONTROLLER_H_
#define ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_INTERACTION_CONTROLLER_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace ash {

class AssistantInteractionModel;

// The interface for the Assistant controller in charge of interactions.
class ASH_PUBLIC_EXPORT AssistantInteractionController {
 public:
  // Returns the singleton instance owned by AssistantController.
  static AssistantInteractionController* Get();

  // Returns a pointer to the underlying model.
  virtual const AssistantInteractionModel* GetModel() const = 0;

  // Returns the TimeDelta since the last Assistant interaction. Note that the
  // last interaction may have been performed in a different user session.
  virtual base::TimeDelta GetTimeDeltaSinceLastInteraction() const = 0;

  // Returns true if the user has had an interaction with the Assistant during
  // this user session.
  virtual bool HasHadInteraction() const = 0;

  // Start Assistant text interaction.
  virtual void StartTextInteraction(const std::string& query,
                                    bool allow_tts,
                                    assistant::AssistantQuerySource source) = 0;

 protected:
  AssistantInteractionController();
  virtual ~AssistantInteractionController();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_INTERACTION_CONTROLLER_H_
