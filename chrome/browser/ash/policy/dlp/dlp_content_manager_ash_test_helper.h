// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_DLP_CONTENT_MANAGER_ASH_TEST_HELPER_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_DLP_CONTENT_MANAGER_ASH_TEST_HELPER_H_

#include <memory>
#include "base/time/time.h"
#include "chrome/browser/ash/policy/dlp/dlp_content_manager_ash.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_contents.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_restriction_set.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"

namespace content {
class WebContents;
}  // namespace content

namespace policy {

class DlpReportingManager;

// This class is an interface to DlpContentManagerAsh and is used in tests to
// access some of its private methods.
class DlpContentManagerAshTestHelper {
 public:
  DlpContentManagerAshTestHelper();
  ~DlpContentManagerAshTestHelper();

  void ChangeConfidentiality(content::WebContents* web_contents,
                             const DlpContentRestrictionSet& restrictions);

  void ChangeVisibility(content::WebContents* web_contents);

  void DestroyWebContents(content::WebContents* web_contents);

  void SetWarnNotifierForTesting(std::unique_ptr<DlpWarnNotifier> notifier);

  void ResetWarnNotifierForTesting();

  bool HasContentCachedForRestriction(
      content::WebContents* web_contents,
      DlpRulesManager::Restriction restriction) const;

  bool HasAnyContentCached() const;

  void EnableScreenShareWarningMode();

  int ActiveWarningDialogsCount() const;

  base::TimeDelta GetPrivacyScreenOffDelay() const;

  DlpContentManagerAsh* GetContentManager() const;
  DlpReportingManager* GetReportingManager() const;

 private:
  DlpContentManagerAsh* manager_;
  DlpReportingManager* reporting_manager_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_DLP_CONTENT_MANAGER_ASH_TEST_HELPER_H_
