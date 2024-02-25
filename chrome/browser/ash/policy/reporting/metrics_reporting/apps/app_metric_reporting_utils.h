// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_APPS_APP_METRIC_REPORTING_UTILS_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_APPS_APP_METRIC_REPORTING_UTILS_H_

#include <optional>

#include "chrome/browser/profiles/profile.h"

namespace reporting {

// Retrieves the app publisher id from the app registry cache using the
// specified profile if one exists.
std::optional<std::string> GetPublisherIdForApp(const std::string& app_id,
                                                Profile* profile);

}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_APPS_APP_METRIC_REPORTING_UTILS_H_
