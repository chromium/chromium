// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_STATE_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_STATE_H_

namespace policy {

// Indicates the current state of the auto-enrollment check.
enum class AutoEnrollmentState {
  // Failed to connect to DMServer or to synchronize the system clock.
  kConnectionError = 2,
  // Connection successful, but the server failed to generate a valid reply.
  kServerError = 3,
  // Check completed successfully, enrollment should be triggered.
  kEnrollment = 4,
  // Check completed successfully, enrollment not applicable.
  kNoEnrollment = 5,
  // Check completed successfully, device is disabled.
  kDisabled = 6,
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_STATE_H_
