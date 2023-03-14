// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_USER_EDUCATION_DELEGATE_H_
#define ASH_USER_EDUCATION_USER_EDUCATION_DELEGATE_H_

#include <string>

#include "ash/ash_export.h"

class AccountId;

namespace user_education {
struct TutorialDescription;
}  // namespace user_education

namespace ash {

// The delegate of the `UserEducationController` which facilitates communication
// between Ash and user education services in the browser.
class ASH_EXPORT UserEducationDelegate {
 public:
  virtual ~UserEducationDelegate() = default;

  // Registers the tutorial defined by the specified `tutorial_id` and
  // `tutorial_description` for the user associated with the given `account_id`.
  virtual void RegisterTutorial(
      const AccountId& account_id,
      const std::string& tutorial_id,
      user_education::TutorialDescription tutorial_description) = 0;
};

}  // namespace ash

#endif  // ASH_USER_EDUCATION_USER_EDUCATION_DELEGATE_H_
