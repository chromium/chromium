// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/child_user_service.h"

#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_controller.h"
#include "content/public/browser/browser_context.h"

namespace chromeos {

ChildUserService::ChildUserService(content::BrowserContext* context) {
  if (AppTimeController::ArePerAppTimeLimitsEnabled())
    app_time_controller_ = std::make_unique<AppTimeController>();
}

ChildUserService::~ChildUserService() = default;

void ChildUserService::Shutdown() {
  app_time_controller_.reset();
}

}  // namespace chromeos
