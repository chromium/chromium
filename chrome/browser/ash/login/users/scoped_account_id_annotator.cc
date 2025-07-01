// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/scoped_account_id_annotator.h"

#include "base/check.h"
#include "base/check_deref.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"

namespace ash {

ScopedAccountIdAnnotator::ScopedAccountIdAnnotator(
    ProfileManager* profile_manager,
    const AccountId& account_id)
    : account_id_(account_id) {
  observation_.Observe(profile_manager);
}

ScopedAccountIdAnnotator::~ScopedAccountIdAnnotator() = default;

void ScopedAccountIdAnnotator::OnProfileCreationStarted(Profile* profile) {
  // Guarantee that only one Profile can have the AccountId.
  CHECK(account_id_.has_value());
  ash::AnnotatedAccountId::Set(profile, *account_id_);
  account_id_.reset();
}

}  // namespace ash
