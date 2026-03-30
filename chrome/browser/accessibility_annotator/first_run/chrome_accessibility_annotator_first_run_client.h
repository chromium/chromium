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

namespace accessibility_annotator {

class ChromeAccessibilityAnnotatorFirstRunClient
    : public AccessibilityAnnotatorFirstRunClient {
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
      FirstRunInvocationSource invocation_source,
      base::OnceCallback<void(InfoResult)> callback) override;
};

}  // namespace accessibility_annotator

#endif  // CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_CHROME_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_CLIENT_H_
