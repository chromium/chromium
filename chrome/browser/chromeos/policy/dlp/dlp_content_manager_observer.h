// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_MANAGER_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_MANAGER_OBSERVER_H_

#include "base/observer_list_types.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"

namespace content {
class WebContents;
}  // namespace content

namespace policy {
// Observer that is notified by the DlpContentManager.
// They are registered using DlpContentManager::AddObserver().
// When registering the observer, it is registered for a specific restriction.
class DlpContentManagerObserver : public base::CheckedObserver {
 public:
  // This method is called when the confidentiality of a tab changes.
  // It will only be called when the level of restriction changed with which
  // this observer was added to the DlpContentManager.
  virtual void OnConfidentialityChanged(
      DlpRulesManager::Level old_restriction_level,
      DlpRulesManager::Level new_restriction_level,
      content::WebContents* web_contents) = 0;
};
}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_MANAGER_OBSERVER_H_
