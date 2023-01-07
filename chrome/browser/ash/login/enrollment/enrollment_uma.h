// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_ENROLLMENT_UMA_H_
#define CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_ENROLLMENT_UMA_H_

#include "chrome/browser/ash/policy/enrollment/enrollment_config.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"

namespace ash {

// Logs an UMA `event` in "Enrollment.*" histogram. Histogram is chosen
// depending on `mode`.
void EnrollmentUMA(policy::MetricEnrollment sample,
                   policy::EnrollmentConfig::Mode mode);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_ENROLLMENT_UMA_H_
