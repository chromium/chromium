// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/family_user_parental_control_metrics.h"

#include "base/check.h"
#include "chrome/browser/ash/child_accounts/child_user_service.h"
#include "chrome/browser/ash/child_accounts/child_user_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "components/user_manager/user_manager.h"

namespace ash {

FamilyUserParentalControlMetrics::FamilyUserParentalControlMetrics(
    Profile* profile)
    : profile_(profile),
      first_report_on_current_device_(
          user_manager::UserManager::Get()->IsCurrentUserNew()) {
  DCHECK(profile_);
  DCHECK(profile_->IsChild());
}

FamilyUserParentalControlMetrics::~FamilyUserParentalControlMetrics() = default;

void FamilyUserParentalControlMetrics::OnNewDay() {
  // Reports Family Link user time limit policy type.
  ChildUserService* child_user_service =
      ChildUserServiceFactory::GetForBrowserContext(profile_);
  DCHECK(child_user_service);
  child_user_service->ReportTimeLimitPolicy();
}

}  // namespace ash
