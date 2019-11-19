// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_POLICY_MIGRATOR_H_
#define CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_POLICY_MIGRATOR_H_

#include "chrome/browser/policy/chrome_extension_policy_migrator.h"

namespace browser_switcher {

extern const char kLBSExtensionId[];

class BrowserSwitcherPolicyMigrator
    : public policy::ChromeExtensionPolicyMigrator {
 public:
  BrowserSwitcherPolicyMigrator();
  ~BrowserSwitcherPolicyMigrator() override;

  void Migrate(policy::PolicyBundle* bundle) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserSwitcherPolicyMigrator);
};

}  // namespace browser_switcher

#endif  // CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_POLICY_MIGRATOR_H_
