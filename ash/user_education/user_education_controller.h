// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_USER_EDUCATION_CONTROLLER_H_
#define ASH_USER_EDUCATION_USER_EDUCATION_CONTROLLER_H_

#include <memory>
#include <set>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/check_op.h"
#include "base/scoped_observation.h"

namespace ash {

class SessionController;
class TutorialController;
class UserEducationDelegate;

// The controller, owned by `Shell`, for user education features in Ash.
class ASH_EXPORT UserEducationController : public SessionObserver {
 public:
  explicit UserEducationController(std::unique_ptr<UserEducationDelegate>);
  UserEducationController(const UserEducationController&) = delete;
  UserEducationController& operator=(const UserEducationController&) = delete;
  ~UserEducationController() override;

  // Returns the singleton instance owned by `Shell`.
  static UserEducationController* Get();

  // TODO(http://b/275616974): Remove after implementing prod controller(s).
  template <typename TutorialControllerType>
  TutorialControllerType* AddTutorialControllerForTesting(
      std::unique_ptr<TutorialControllerType> tutorial_controller) {
    // Ensure that all tutorial controllers have been added prior to the primary
    // user session being added. Otherwise, the tutorials for those controllers
    // will not be registered with user education services in the browser.
    CHECK(session_observation_.IsObserving());
    auto* tutorial_controller_ptr = tutorial_controller.get();
    tutorial_controllers_.emplace(std::move(tutorial_controller));
    return tutorial_controller_ptr;
  }

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
