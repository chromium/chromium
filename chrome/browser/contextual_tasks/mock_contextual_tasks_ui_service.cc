// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/mock_contextual_tasks_ui_service.h"

#include "chrome/browser/contextual_tasks/contextual_tasks_cookie_synchronizer.h"

namespace contextual_tasks {

MockContextualTasksUiService::MockContextualTasksUiService()
    : ContextualTasksUiService(/*profile=*/nullptr,
                               /*delegate=*/nullptr,
                               /*contextual_tasks_service=*/nullptr,
                               /*identity_manager=*/nullptr,
                               /*aim_eligibility_service=*/nullptr,
                               /*cookie_synchronizer=*/nullptr) {}

MockContextualTasksUiService::MockContextualTasksUiService(
    Profile* profile,
    ContextualTasksService* service)
    : MockContextualTasksUiService(profile,
                                   service,
                                   /*identity_manager=*/nullptr,
                                   /*aim_eligibility_service=*/nullptr) {}

MockContextualTasksUiService::MockContextualTasksUiService(
    Profile* profile,
    ContextualTasksService* service,
    signin::IdentityManager* identity_manager,
    AimEligibilityService* aim_eligibility_service)
    : ContextualTasksUiService(profile,
                               nullptr,
                               service,
                               identity_manager,
                               aim_eligibility_service,
                               /*cookie_synchronizer=*/nullptr) {}

MockContextualTasksUiService::~MockContextualTasksUiService() = default;

}  // namespace contextual_tasks
