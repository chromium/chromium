// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_MANAGER_LACROS_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_MANAGER_LACROS_H_

#include "chrome/browser/chromeos/policy/dlp/dlp_content_observer.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_restriction_set.h"

class GURL;

namespace content {
class WebContents;
}  // namespace content

namespace policy {

// LaCros-wide class that tracks the set of currently known confidential
// WebContents and whether any of them are currently visible.
class DlpContentManagerLacros : public DlpContentObserver {
 public:
  // Creates the instance if not yet created.
  // There will always be a single instance created on the first access.
  static DlpContentManagerLacros* Get();

  ~DlpContentManagerLacros() override = default;

 private:
  // DlpContentObserver overrides:
  void OnConfidentialityChanged(
      content::WebContents* web_contents,
      const DlpContentRestrictionSet& restriction_set) override;
  void OnWebContentsDestroyed(content::WebContents* web_contents) override;
  DlpContentRestrictionSet GetRestrictionSetForURL(
      const GURL& url) const override;
  void OnVisibilityChanged(content::WebContents* web_contents) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_MANAGER_LACROS_H_
