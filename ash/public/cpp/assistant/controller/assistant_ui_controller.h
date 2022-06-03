// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_UI_CONTROLLER_H_
#define ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_UI_CONTROLLER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/callback_helpers.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {
namespace assistant {
enum class AssistantEntryPoint;
enum class AssistantExitPoint;
}  // namespace assistant
}  // namespace chromeos

namespace ash {

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

  // Invoke to show/toggle Assistant UI.
  virtual void ShowUi(chromeos::assistant::AssistantEntryPoint) = 0;
  virtual void ToggleUi(
      absl::optional<chromeos::assistant::AssistantEntryPoint>,
      absl::optional<chromeos::assistant::AssistantExitPoint>) = 0;

  // Returns a closure to close Assistant UI. If the return value is ignored,
  // the Assistant UI is closed instantly; otherwise, the UI is in closing
  // state until the closure is run.
  virtual absl::optional<base::ScopedClosureRunner> CloseUi(
      chromeos::assistant::AssistantExitPoint) = 0;

 protected:
  AssistantUiController();
  virtual ~AssistantUiController();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_UI_CONTROLLER_H_
