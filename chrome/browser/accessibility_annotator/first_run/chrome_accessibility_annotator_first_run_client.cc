// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility_annotator/first_run/chrome_accessibility_annotator_first_run_client.h"

#include "base/functional/callback.h"
#include "components/accessibility_annotator/first_run/accessibility_annotator_first_run_types.h"

namespace accessibility_annotator {
ChromeAccessibilityAnnotatorFirstRunClient::
    ChromeAccessibilityAnnotatorFirstRunClient() = default;

ChromeAccessibilityAnnotatorFirstRunClient::
    ~ChromeAccessibilityAnnotatorFirstRunClient() = default;

void ChromeAccessibilityAnnotatorFirstRunClient::ShowRemoteAnnotatorInfo(
    content::WebContents* web_contents,
    FirstRunInvocationSource invocation_source,
    base::OnceCallback<void(InfoResult)> callback) {
  // TODO(b/489414512): Implement this.
  std::move(callback).Run(InfoResult::kNotAcknowledged);
}

}  // namespace accessibility_annotator
