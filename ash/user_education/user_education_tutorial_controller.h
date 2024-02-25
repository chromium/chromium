// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_USER_EDUCATION_TUTORIAL_CONTROLLER_H_
#define ASH_USER_EDUCATION_USER_EDUCATION_TUTORIAL_CONTROLLER_H_

#include <optional>

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"

namespace ui {
class ElementContext;
}  // namespace ui

namespace user_education {
struct TutorialDescription;
}  // namespace user_education

namespace ash {

class UserEducationDelegate;
class UserEducationPrivateApiKey;
enum class TutorialId;

// The singleton controller, owned by the `UserEducationController`, responsible
// for creation/management of tutorials.
class ASH_EXPORT UserEducationTutorialController {
 public:
  explicit UserEducationTutorialController(UserEducationDelegate* delegate);
  UserEducationTutorialController(const UserEducationTutorialController&) =
      delete;
  UserEducationTutorialController& operator=(
      const UserEducationTutorialController&) = delete;
  ~UserEducationTutorialController();

  // Returns the singleton instance owned by the `UserEducationController`.
  // NOTE: Exists if and only if user education features are enabled.
  static UserEducationTutorialController* Get();

  // Returns whether a tutorial is registered for the specified `tutorial_id`.
  // NOTE: Currently only the primary user profile is supported.
  bool IsTutorialRegistered(TutorialId tutorial_id) const;

  // Registers the tutorial with the specified `tutorial_id`.
  // NOTE: Currently only the primary user profile is supported.
  void RegisterTutorial(
      UserEducationPrivateApiKey,
      TutorialId tutorial_id,
      user_education::TutorialDescription tutorial_description);

  // Starts the tutorial previously registered with the specified `tutorial_id`.
  // Any running tutorial is cancelled. One of either `completed_callback` or
  // `aborted_callback` will be run on tutorial finish.
  // NOTE: Currently only the primary user profile is supported.
  void StartTutorial(UserEducationPrivateApiKey,
                     TutorialId tutorial_id,
                     ui::ElementContext element_context,
                     base::OnceClosure completed_callback,
                     base::OnceClosure aborted_callback);

  // Aborts the currently running tutorial. If `tutorial_id` is given, will only
  // abort the tutorial if it matches the id. If no `tutorial_id` is given, it
  // aborts any running tutorial whether it was started by this controller or
  // not. Any `aborted_callback` passed in at the time of start will be called.
  // NOTE: Currently only the primary user profile is supported.
  void AbortTutorial(UserEducationPrivateApiKey,
                     std::optional<TutorialId> tutorial_id = std::nullopt);

 private:
  // The delegate owned by the `UserEducationController` which facilitates
  // communication between Ash and user education services in the browser.
  const raw_ptr<UserEducationDelegate> delegate_;
};

}  // namespace ash

#endif  // ASH_USER_EDUCATION_USER_EDUCATION_TUTORIAL_CONTROLLER_H_
