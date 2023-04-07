// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_USER_EDUCATION_DELEGATE_H_
#define ASH_USER_EDUCATION_USER_EDUCATION_DELEGATE_H_

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"

class AccountId;

namespace ui {
class ElementContext;
}  // namespace ui

namespace user_education {
struct TutorialDescription;
}  // namespace user_education

namespace ash {

enum class TutorialId;

// The delegate of the `UserEducationController` which facilitates communication
// between Ash and user education services in the browser.
class ASH_EXPORT UserEducationDelegate {
 public:
  virtual ~UserEducationDelegate() = default;

  // Registers the tutorial defined by the specified `tutorial_id` and
  // `tutorial_description` for the user associated with the given `account_id`.
  // NOTE: Currently only the primary user profile is supported.
  virtual void RegisterTutorial(
      const AccountId& account_id,
      TutorialId tutorial_id,
      user_education::TutorialDescription tutorial_description) = 0;

  // Starts the tutorial previously registered with the specified `tutorial_id`
  // for the user associated with the given `account_id`. Any running tutorial
  // is cancelled. One of either `completed_callback` or `aborted_callback` will
  // be run on tutorial finish.
  // NOTE: Currently only the primary user profile is supported.
  virtual void StartTutorial(const AccountId& account_id,
                             TutorialId tutorial_id,
                             ui::ElementContext element_context,
                             base::OnceClosure completed_callback,
                             base::OnceClosure aborted_callback) = 0;
};

}  // namespace ash

#endif  // ASH_USER_EDUCATION_USER_EDUCATION_DELEGATE_H_
