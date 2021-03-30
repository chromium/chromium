// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager_test_helper.h"

namespace policy {

DlpContentManagerTestHelper::DlpContentManagerTestHelper() {
  manager_ = DlpContentManager::Get();
  DCHECK(manager_);
}

void DlpContentManagerTestHelper::ChangeConfidentiality(
    content::WebContents* web_contents,
    DlpContentRestrictionSet restrictions) {
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

}  // namespace policy
