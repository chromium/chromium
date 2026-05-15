// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/mock_contextual_tasks_ui_service.h"

#include "chrome/browser/contextual_tasks/contextual_tasks_cookie_synchronizer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_eligibility_manager.h"
#include "chrome/browser/profiles/profile.h"

namespace contextual_tasks {

MockContextualTasksUiService::MockContextualTasksUiService(
    Profile* profile,
    ContextualTasksService* service,
    signin::IdentityManager* identity_manager,
    AimEligibilityService* aim_eligibility_service,
    std::unique_ptr<ContextualTasksEligibilityManager> eligibility_manager,
    std::unique_ptr<ContextualTasksCookieSynchronizer> cookie_synchronizer)
    : ContextualTasksUiService(profile,
                               nullptr,
                               service,
                               identity_manager,
                               aim_eligibility_service,
                               std::move(eligibility_manager),
                               std::move(cookie_synchronizer)) {}

MockContextualTasksUiService::~MockContextualTasksUiService() = default;

}  // namespace contextual_tasks
