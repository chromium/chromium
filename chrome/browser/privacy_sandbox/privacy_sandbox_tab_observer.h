// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_TAB_OBSERVER_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_TAB_OBSERVER_H_

#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace privacy_sandbox {

class PrivacySandboxTabObserver : public content::WebContentsObserver {
 public:
  explicit PrivacySandboxTabObserver(content::WebContents* web_contents);
  ~PrivacySandboxTabObserver() override;

 private:
  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

  // Manages HaTS logic.
  void MaybeTriggerSurveys(content::WebContents* web_contents);
  // Tracks if a NTP has been seen before.
  bool has_seen_ntp_ = false;
};

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_TAB_OBSERVER_H_
