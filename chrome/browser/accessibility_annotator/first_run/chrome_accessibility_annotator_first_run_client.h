// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_CHROME_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_CLIENT_H_
#define CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_CHROME_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_CLIENT_H_

#include "base/functional/callback.h"
#include "components/accessibility_annotator/first_run/accessibility_annotator_first_run_client.h"

namespace content {
class WebContents;
}

class ChromeAccessibilityAnnotatorFirstRunClient
    : public accessibility_annotator::AccessibilityAnnotatorFirstRunClient {
 public:
  ChromeAccessibilityAnnotatorFirstRunClient();
  ChromeAccessibilityAnnotatorFirstRunClient(
      const ChromeAccessibilityAnnotatorFirstRunClient&) = delete;
  ChromeAccessibilityAnnotatorFirstRunClient& operator=(
      const ChromeAccessibilityAnnotatorFirstRunClient&) = delete;
  ~ChromeAccessibilityAnnotatorFirstRunClient() override;

  // AccessibilityAnnotatorFirstRunClient:
  void ShowRemoteAnnotatorInfo(
      content::WebContents* web_contents,
      accessibility_annotator::FirstRunInvocationSource invocation_source,
      base::OnceCallback<void(accessibility_annotator::InfoResult)> callback)
      override;
};

#endif  // CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_CHROME_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_CLIENT_H_
