// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_UI_CONTROLLER_H_
#define ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_UI_CONTROLLER_H_

#include <optional>

#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback_helpers.h"

namespace ash {

namespace assistant {
enum class AssistantEntryPoint;
enum class AssistantExitPoint;
}  // namespace assistant

class AssistantUiModel;

// The interface for the Assistant controller in charge of UI.
class ASH_PUBLIC_EXPORT AssistantUiController {
 public:
  // Returns the singleton instance owned by AssistantController.
  static AssistantUiController* Get();

  // Returns a pointer to the underlying model.
  virtual const AssistantUiModel* GetModel() const = 0;

  // Returns the number of user sessions where Assistant onboarding was shown.
  virtual int GetNumberOfSessionsWhereOnboardingShown() const = 0;

  // Returns true if the user has been shown Assistant onboarding in this user
  // session.
  virtual bool HasShownOnboarding() const = 0;

  // Sets keyboard traversal mode for the underlying model.
  virtual void SetKeyboardTraversalMode(bool) = 0;

  // Invoke to show/toggle Assistant UI.
  virtual void ShowUi(assistant::AssistantEntryPoint) = 0;
  virtual void ToggleUi(std::optional<assistant::AssistantEntryPoint>,
                        std::optional<assistant::AssistantExitPoint>) = 0;

  // Returns a closure to close Assistant UI. If the return value is ignored,
  // the Assistant UI is closed instantly; otherwise, the UI is in closing
  // state until the closure is run.
  virtual std::optional<base::ScopedClosureRunner> CloseUi(
      assistant::AssistantExitPoint) = 0;

  // Sets current AppListBubbleWidth. AssistantCardElement needs to know the
  // width of AppListBubbleWidth to render its html content.
  // AssistantCardElement will take the value via AssistantUiModel.
  virtual void SetAppListBubbleWidth(int width) = 0;

 protected:
  AssistantUiController();
  virtual ~AssistantUiController();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_UI_CONTROLLER_H_
