// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_TEST_DLP_CONTENT_MANAGER_TEST_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_TEST_DLP_CONTENT_MANAGER_TEST_HELPER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_contents.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_restriction_set.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/dlp/dlp_content_manager_ash.h"
#endif

namespace content {
class WebContents;
}  // namespace content

namespace data_controls {
class DlpReportingManager;
}  // namespace data_controls

namespace policy {

// This class is an interface to DlpContentManager and is used in tests to
// access some of its private methods.
class DlpContentManagerTestHelper {
 public:
  DlpContentManagerTestHelper();
  ~DlpContentManagerTestHelper();

  void ChangeConfidentiality(content::WebContents* web_contents,
                             DlpContentRestrictionSet restrictions);

  // To be called when confidentiality for |web_contents| needs to be changed
  // but without reacting to the change.
  void UpdateConfidentiality(content::WebContents* web_contents,
                             DlpContentRestrictionSet restrictions);

  void ChangeVisibility(content::WebContents* web_contents);

  void DestroyWebContents(content::WebContents* web_contents);

  void CheckRunningScreenShares();

  void SetWarnNotifierForTesting(std::unique_ptr<DlpWarnNotifier> notifier);

  void ResetWarnNotifierForTesting();

  int ActiveWarningDialogsCount() const;

  const std::vector<std::unique_ptr<DlpContentManager::ScreenShareInfo>>&
  GetRunningScreenShares() const;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::optional<DlpContentManagerAsh::VideoCaptureInfo>
  GetRunningVideoCaptureInfo() const;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::TimeDelta GetPrivacyScreenOffDelay() const;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  void SetScreenShareResumeDelay(base::TimeDelta delay) const;

  DlpContentManager* GetContentManager() const;
  data_controls::DlpReportingManager* GetReportingManager() const;

 private:
  raw_ptr<DlpContentManager, DanglingUntriaged> manager_;
  raw_ptr<data_controls::DlpReportingManager, DanglingUntriaged>
      reporting_manager_;
  raw_ptr<ScopedDlpContentObserverForTesting, DanglingUntriaged>
      scoped_dlp_content_observer_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_TEST_DLP_CONTENT_MANAGER_TEST_HELPER_H_
