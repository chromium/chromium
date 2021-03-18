// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limits/app_activity_report_interface.h"

#include "chrome/browser/chromeos/child_accounts/child_user_service.h"
#include "chrome/browser/chromeos/child_accounts/child_user_service_factory.h"
#include "chrome/browser/profiles/profile.h"

namespace chromeos {
namespace app_time {

// static
AppActivityReportInterface* AppActivityReportInterface::Get(Profile* profile) {
  return static_cast<AppActivityReportInterface*>(
      ChildUserServiceFactory::GetForBrowserContext(profile));
}

AppActivityReportInterface::~AppActivityReportInterface() = default;

}  // namespace app_time
}  // namespace chromeos
