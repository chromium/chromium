// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_MANAGER_TEST_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_MANAGER_TEST_HELPER_H_

#include "base/time/time.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_restriction_set.h"

class GURL;

namespace content {
class WebContents;
}  // namespace content

namespace policy {

// This class is an interface to DlpContentManager and is used in tests to
// access some of it's private methods.
class DlpContentManagerTestHelper {
 public:
  DlpContentManagerTestHelper();

  void ChangeConfidentiality(content::WebContents* web_contents,
                             DlpContentRestrictionSet restrictions);

  void ChangeVisibility(content::WebContents* web_contents);

  void DestroyWebContents(content::WebContents* web_contents);

  base::TimeDelta GetPrivacyScreenOffDelay() const;

  DlpContentRestrictionSet GetRestrictionSetForURL(const GURL& url) const;

 private:
  DlpContentManager* manager_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_MANAGER_TEST_HELPER_H_
