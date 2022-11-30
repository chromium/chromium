// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_SUGGESTIONS_CONTROLLER_H_
#define ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_SUGGESTIONS_CONTROLLER_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

class AssistantSuggestionsModel;

// The interface for the Assistant controller in charge of suggestions.
class ASH_PUBLIC_EXPORT AssistantSuggestionsController {
 public:
  // Returns the singleton instance owned by AssistantController.
  static AssistantSuggestionsController* Get();

  // Returns a pointer to the underlying model.
  virtual const AssistantSuggestionsModel* GetModel() const = 0;

 protected:
  AssistantSuggestionsController();
  virtual ~AssistantSuggestionsController();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_SUGGESTIONS_CONTROLLER_H_
