// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_RESTORE_TEST_UTILS_H_
#define CHROME_BROWSER_SESSIONS_SESSION_RESTORE_TEST_UTILS_H_

#include "chrome/browser/resource_coordinator/session_restore_policy.h"

namespace testing {

// SessionRestorePolicy that always allow tabs to load.
class AlwayLoadSessionRestorePolicy
    : public resource_coordinator::SessionRestorePolicy {
 public:
  AlwayLoadSessionRestorePolicy() = default;
  ~AlwayLoadSessionRestorePolicy() override = default;

  // Always allow tabs to load so we can test the behavior of SessionRestore
  // independently from the policy logic.
  bool ShouldLoad(content::WebContents* contents) const override;
};

class ScopedAlwaysLoadSessionRestoreTestPolicy {
 public:
  ScopedAlwaysLoadSessionRestoreTestPolicy();

  ScopedAlwaysLoadSessionRestoreTestPolicy(
      const ScopedAlwaysLoadSessionRestoreTestPolicy&) = delete;
  ScopedAlwaysLoadSessionRestoreTestPolicy& operator=(
      const ScopedAlwaysLoadSessionRestoreTestPolicy&) = delete;

  ~ScopedAlwaysLoadSessionRestoreTestPolicy();

 private:
  AlwayLoadSessionRestorePolicy policy_;
};

}  // namespace testing

#endif  // CHROME_BROWSER_SESSIONS_SESSION_RESTORE_TEST_UTILS_H_
