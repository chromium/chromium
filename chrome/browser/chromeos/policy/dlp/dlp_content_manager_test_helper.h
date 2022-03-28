// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_MANAGER_TEST_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_MANAGER_TEST_HELPER_H_

#include <memory>
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_contents.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_restriction_set.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"

namespace content {
class WebContents;
}  // namespace content

namespace policy {

class DlpReportingManager;

// This class is an interface to DlpContentManager and is used in tests to
// access some of its private methods.
class DlpContentManagerTestHelper {
 public:
  DlpContentManagerTestHelper();
  ~DlpContentManagerTestHelper();

  void ChangeConfidentiality(content::WebContents* web_contents,
                             const DlpContentRestrictionSet& restrictions);

  void ChangeVisibility(content::WebContents* web_contents);

  void DestroyWebContents(content::WebContents* web_contents);

  void CheckRunningScreenShares();

  void SetWarnNotifierForTesting(std::unique_ptr<DlpWarnNotifier> notifier);

  void ResetWarnNotifierForTesting();

  bool HasContentCachedForRestriction(
      content::WebContents* web_contents,
      DlpRulesManager::Restriction restriction) const;

  bool HasAnyContentCached() const;

  int ActiveWarningDialogsCount() const;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::TimeDelta GetPrivacyScreenOffDelay() const;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  DlpContentManager* GetContentManager() const;
  DlpReportingManager* GetReportingManager() const;

 private:
  DlpContentManager* manager_;
  DlpReportingManager* reporting_manager_;
  ScopedDlpContentObserverForTesting* scoped_dlp_content_observer_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_MANAGER_TEST_HELPER_H_
