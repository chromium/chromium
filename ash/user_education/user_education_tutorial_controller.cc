// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/user_education_tutorial_controller.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/user_education/user_education_delegate.h"
#include "ash/user_education/user_education_private_api_key.h"
#include "ash/user_education/user_education_util.h"
#include "base/check_op.h"
#include "components/account_id/account_id.h"
#include "components/user_education/common/tutorial_description.h"

namespace ash {
namespace {

// The singleton instance owned by the `UserEducationController`.
UserEducationTutorialController* g_instance = nullptr;

// Helpers ---------------------------------------------------------------------

AccountId GetActiveAccountId() {
  return Shell::Get()->session_controller()->GetActiveAccountId();
}

}  // namespace

// UserEducationTutorialController ---------------------------------------------

UserEducationTutorialController::UserEducationTutorialController(
    UserEducationDelegate* delegate)
    : delegate_(std::move(delegate)) {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

UserEducationTutorialController::~UserEducationTutorialController() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
UserEducationTutorialController* UserEducationTutorialController::Get() {
  return g_instance;
}

bool UserEducationTutorialController::IsTutorialRegistered(
    TutorialId tutorial_id) const {
  // NOTE: User education in Ash is currently only supported for the primary
  // user profile. This is a self-imposed restriction.
  const AccountId account_id = GetActiveAccountId();
  CHECK(user_education_util::IsPrimaryAccountId(account_id));
  return delegate_->IsTutorialRegistered(account_id, tutorial_id);
}

void UserEducationTutorialController::RegisterTutorial(
    UserEducationPrivateApiKey,
    TutorialId tutorial_id,
    user_education::TutorialDescription tutorial_description) {
  // NOTE: User education in Ash is currently only supported for the primary
  // user profile. This is a self-imposed restriction.
  const AccountId account_id = GetActiveAccountId();
  CHECK(user_education_util::IsPrimaryAccountId(account_id));
  delegate_->RegisterTutorial(account_id, tutorial_id,
                              std::move(tutorial_description));
}

void UserEducationTutorialController::StartTutorial(
    UserEducationPrivateApiKey,
    TutorialId tutorial_id,
    ui::ElementContext element_context,
    base::OnceClosure completed_callback,
    base::OnceClosure aborted_callback) {
  // NOTE: User education in Ash is currently only supported for the primary
  // user profile. This is a self-imposed restriction.
  const AccountId account_id = GetActiveAccountId();
  CHECK(user_education_util::IsPrimaryAccountId(account_id));
  delegate_->StartTutorial(account_id, tutorial_id, element_context,
                           std::move(completed_callback),
                           std::move(aborted_callback));
}

void UserEducationTutorialController::AbortTutorial(
    UserEducationPrivateApiKey,
    std::optional<TutorialId> tutorial_id) {
  // NOTE: User education in Ash is currently only supported for the primary
  // user profile. This is a self-imposed restriction.
  const AccountId account_id = GetActiveAccountId();
  CHECK(user_education_util::IsPrimaryAccountId(account_id));
  delegate_->AbortTutorial(account_id, tutorial_id);
}

}  // namespace ash
