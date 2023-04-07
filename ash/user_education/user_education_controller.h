// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_USER_EDUCATION_CONTROLLER_H_
#define ASH_USER_EDUCATION_USER_EDUCATION_CONTROLLER_H_

#include <memory>
#include <set>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/user_education/user_education_private_api_key.h"
#include "base/functional/callback_forward.h"
#include "base/scoped_observation.h"

namespace ui {
class ElementContext;
}  // namespace ui

namespace ash {

class SessionController;
class TutorialController;
class UserEducationDelegate;

enum class TutorialId;

// The controller, owned by `Shell`, for user education features in Ash.
class ASH_EXPORT UserEducationController : public SessionObserver {
 public:
  explicit UserEducationController(std::unique_ptr<UserEducationDelegate>);
  UserEducationController(const UserEducationController&) = delete;
  UserEducationController& operator=(const UserEducationController&) = delete;
  ~UserEducationController() override;

  // Returns the singleton instance owned by `Shell`.
  // NOTE: Exists if and only if user education features are enabled.
  static UserEducationController* Get();

  // Starts the tutorial previously registered with the specified `tutorial_id`.
  // Any running tutorial is cancelled. One of either `completed_callback` or
  // `aborted_callback` will be run on tutorial finish.
  // NOTE: Currently only the primary user profile is supported.
  void StartTutorial(UserEducationPrivateApiKey,
                     TutorialId tutorial_id,
                     ui::ElementContext element_context,
                     base::OnceClosure completed_callback,
                     base::OnceClosure aborted_callback);

 private:
  // SessionObserver:
  void OnChromeTerminating() override;
  void OnUserSessionAdded(const AccountId& account_id) override;

  // The delegate  which facilitates communication between Ash and user
  // education services in the browser.
  std::unique_ptr<UserEducationDelegate> delegate_;

  // The collection of controllers responsible for specific feature tutorials.
  std::set<std::unique_ptr<TutorialController>> tutorial_controllers_;

  // Sessions are observed only until the primary user session is added at which
  // point tutorials are registered with user education services in the browser.
  base::ScopedObservation<SessionController, SessionObserver>
      session_observation_{this};
};

}  // namespace ash

#endif  // ASH_USER_EDUCATION_USER_EDUCATION_CONTROLLER_H_
