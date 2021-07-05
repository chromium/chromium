// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_content_manager_test_helper.h"
#include "chrome/browser/ash/policy/dlp/dlp_reporting_manager.h"

namespace policy {

DlpContentManagerTestHelper::DlpContentManagerTestHelper() {
  manager_ = new DlpContentManager();
  DCHECK(manager_);
  reporting_manager_ = new DlpReportingManager();
  DCHECK(reporting_manager_);
  manager_->SetReportingManagerForTesting(reporting_manager_);
  DlpContentManager::SetDlpContentManagerForTesting(manager_);
}

DlpContentManagerTestHelper::~DlpContentManagerTestHelper() {
  delete reporting_manager_;
}

void DlpContentManagerTestHelper::ChangeConfidentiality(
    content::WebContents* web_contents,
    const DlpContentRestrictionSet& restrictions) {
  DCHECK(manager_);
  manager_->OnConfidentialityChanged(web_contents, restrictions);
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

base::TimeDelta DlpContentManagerTestHelper::GetPrivacyScreenOffDelay() const {
  DCHECK(manager_);
  return manager_->GetPrivacyScreenOffDelayForTesting();
}

DlpContentRestrictionSet DlpContentManagerTestHelper::GetRestrictionSetForURL(
    const GURL& url) const {
  DCHECK(manager_);
  return manager_->GetRestrictionSetForURL(url);
}

DlpContentManager* DlpContentManagerTestHelper::GetContentManager() const {
  return manager_;
}

DlpReportingManager* DlpContentManagerTestHelper::GetReportingManager() const {
  return manager_->reporting_manager_;
}

}  // namespace policy
