// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/enrollment/mock_enrollment_launcher.h"

namespace ash {

MockEnrollmentLauncher::MockEnrollmentLauncher() = default;

MockEnrollmentLauncher::~MockEnrollmentLauncher() = default;

EnrollmentLauncher::EnrollmentStatusConsumer*
MockEnrollmentLauncher::status_consumer() const {
  return EnrollmentLauncher::status_consumer();
}

}  // namespace ash
