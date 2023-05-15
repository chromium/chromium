// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager_test_helper.h"

#include <memory>

#include "chrome/browser/chromeos/policy/dlp/dialogs/dlp_warn_notifier.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_reporting_manager.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/dlp/dlp_content_manager_ash.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager_lacros.h"
#endif

namespace policy {

DlpContentManagerTestHelper::DlpContentManagerTestHelper() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  manager_ = new DlpContentManagerAsh();
#else
  manager_ = new DlpContentManagerLacros();
#endif
  DCHECK(manager_);
  reporting_manager_ = new DlpReportingManager();
  DCHECK(reporting_manager_);
  manager_->SetReportingManagerForTesting(reporting_manager_);
  manager_->SetWarnNotifierForTesting(std::make_unique<DlpWarnNotifier>());
  scoped_dlp_content_observer_ =
      new ScopedDlpContentObserverForTesting(manager_);
}

DlpContentManagerTestHelper::~DlpContentManagerTestHelper() {
  delete scoped_dlp_content_observer_;
  delete reporting_manager_;
  delete manager_;
}

void DlpContentManagerTestHelper::ChangeConfidentiality(
    content::WebContents* web_contents,
    const DlpContentRestrictionSet& restrictions) {
  DCHECK(manager_);
  manager_->OnConfidentialityChanged(web_contents, restrictions);
}

void DlpContentManagerTestHelper::UpdateConfidentiality(
    content::WebContents* web_contents,
    const DlpContentRestrictionSet& restrictions) {
  DCHECK(manager_);
  manager_->UpdateConfidentiality(web_contents, restrictions);
}

void DlpContentManagerTestHelper::ChangeVisibility(
    content::WebContents* web_contents) {
  DCHECK(manager_);
  manager_->OnVisibilityChanged(web_contents);
}

void DlpContentManagerTestHelper::DestroyWebContents(
    content::WebContents* web_contents) {
  DCHECK(manager_);
  manager_->OnWebContentsDestroyed(web_contents);
}

void DlpContentManagerTestHelper::CheckRunningScreenShares() {
  DCHECK(manager_);
  manager_->CheckRunningScreenShares();
}

void DlpContentManagerTestHelper::SetWarnNotifierForTesting(
    std::unique_ptr<DlpWarnNotifier> notifier) {
  DCHECK(manager_);
  manager_->SetWarnNotifierForTesting(std::move(notifier));
}

void DlpContentManagerTestHelper::ResetWarnNotifierForTesting() {
  DCHECK(manager_);
  manager_->ResetWarnNotifierForTesting();
}

int DlpContentManagerTestHelper::ActiveWarningDialogsCount() const {
  DCHECK(manager_);
  return manager_->warn_notifier_->ActiveWarningDialogsCountForTesting();
}

const std::vector<std::unique_ptr<DlpContentManager::ScreenShareInfo>>&
DlpContentManagerTestHelper::GetRunningScreenShares() const {
  DCHECK(manager_);
  return manager_->running_screen_shares_;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
absl::optional<DlpContentManagerAsh::VideoCaptureInfo>
DlpContentManagerTestHelper::GetRunningVideoCaptureInfo() const {
  DCHECK(manager_);
  return static_cast<DlpContentManagerAsh*>(manager_)
      ->running_video_capture_info_;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
base::TimeDelta DlpContentManagerTestHelper::GetPrivacyScreenOffDelay() const {
  DCHECK(manager_);
  return static_cast<DlpContentManagerAsh*>(manager_)
      ->GetPrivacyScreenOffDelayForTesting();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void DlpContentManagerTestHelper::SetScreenShareResumeDelay(
    base::TimeDelta delay) const {
  DCHECK(manager_);
  manager_->SetScreenShareResumeDelayForTesting(delay);
}

DlpContentManager* DlpContentManagerTestHelper::GetContentManager() const {
  return manager_;
}

DlpReportingManager* DlpContentManagerTestHelper::GetReportingManager() const {
  return manager_->reporting_manager_;
}

}  // namespace policy
