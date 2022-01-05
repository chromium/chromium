// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_content_manager_ash_test_helper.h"

#include <memory>

#include "chrome/browser/ash/policy/dlp/dlp_content_manager_ash.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_reporting_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_warn_notifier.h"

namespace policy {

DlpContentManagerAshTestHelper::DlpContentManagerAshTestHelper() {
  manager_ = new DlpContentManagerAsh();
  DCHECK(manager_);
  reporting_manager_ = new DlpReportingManager();
  DCHECK(reporting_manager_);
  manager_->SetReportingManagerForTesting(reporting_manager_);
  DlpContentManagerAsh::SetDlpContentManagerAshForTesting(manager_);
}

DlpContentManagerAshTestHelper::~DlpContentManagerAshTestHelper() {
  delete reporting_manager_;
}

void DlpContentManagerAshTestHelper::ChangeConfidentiality(
    content::WebContents* web_contents,
    const DlpContentRestrictionSet& restrictions) {
  DCHECK(manager_);
  manager_->OnConfidentialityChanged(web_contents, restrictions);
}

void DlpContentManagerAshTestHelper::ChangeVisibility(
    content::WebContents* web_contents) {
  DCHECK(manager_);
  manager_->OnVisibilityChanged(web_contents);
}

void DlpContentManagerAshTestHelper::DestroyWebContents(
    content::WebContents* web_contents) {
  DCHECK(manager_);
  manager_->OnWebContentsDestroyed(web_contents);
}

void DlpContentManagerAshTestHelper::SetWarnNotifierForTesting(
    std::unique_ptr<DlpWarnNotifier> notifier) {
  DCHECK(manager_);
  manager_->SetWarnNotifierForTesting(std::move(notifier));
}

void DlpContentManagerAshTestHelper::ResetWarnNotifierForTesting() {
  DCHECK(manager_);
  manager_->ResetWarnNotifierForTesting();
}

bool DlpContentManagerAshTestHelper::HasContentCachedForRestriction(
    content::WebContents* web_contents,
    DlpRulesManager::Restriction restriction) const {
  DCHECK(manager_);
  return manager_->user_allowed_contents_cache_.Contains(web_contents,
                                                         restriction);
}

bool DlpContentManagerAshTestHelper::HasAnyContentCached() const {
  DCHECK(manager_);
  return manager_->user_allowed_contents_cache_.GetSizeForTesting() != 0;
}

void DlpContentManagerAshTestHelper::EnableScreenShareWarningMode() {
  DCHECK(manager_);
  manager_->SetIsScreenShareWarningModeEnabledForTesting(/*is_enabled=*/true);
}

int DlpContentManagerAshTestHelper::ActiveWarningDialogsCount() const {
  DCHECK(manager_);
  return manager_->warn_notifier_->ActiveWarningDialogsCountForTesting();
}

base::TimeDelta DlpContentManagerAshTestHelper::GetPrivacyScreenOffDelay()
    const {
  DCHECK(manager_);
  return manager_->GetPrivacyScreenOffDelayForTesting();
}

DlpContentManagerAsh* DlpContentManagerAshTestHelper::GetContentManager()
    const {
  return manager_;
}

DlpReportingManager* DlpContentManagerAshTestHelper::GetReportingManager()
    const {
  return manager_->reporting_manager_;
}

}  // namespace policy
