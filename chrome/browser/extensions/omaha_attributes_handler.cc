// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/omaha_attributes_handler.h"

#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

// static
void OmahaAttributesHandler::ReportExtensionDisabledRemotely(
    bool should_be_remotely_disabled,
    ExtensionUpdateCheckDataKey reason) {
  // Report that the extension is newly disabled due to Omaha attributes.
  if (should_be_remotely_disabled)
    base::UmaHistogramEnumeration("Extensions.ExtensionDisabledRemotely",
                                  reason);

  // Report that the extension has added a new disable reason.
  base::UmaHistogramEnumeration("Extensions.ExtensionAddDisabledRemotelyReason",
                                reason);
}

// static
void OmahaAttributesHandler::ReportNoUpdateCheckKeys() {
  base::UmaHistogramEnumeration("Extensions.ExtensionDisabledRemotely",
                                ExtensionUpdateCheckDataKey::kNoKey);
}

// static
void OmahaAttributesHandler::ReportReenableExtensionFromMalware() {
  base::UmaHistogramCounts100("Extensions.ExtensionReenabledRemotely", 1);
}

void OmahaAttributesHandler::PerformActionBasedOnOmahaAttributes(
    const base::Value& attributes) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ReportPolicyViolationUWSOmahaAttributes(attributes);
  // TODO(crbug.com/1180996): Perform action based on the attributes.
}

void OmahaAttributesHandler::ReportPolicyViolationUWSOmahaAttributes(
    const base::Value& attributes) {
  const base::Value* uws_value = attributes.FindKey("_potentially_uws");
  if (uws_value != nullptr && uws_value->GetBool()) {
    ReportExtensionDisabledRemotely(
        /*should_be_remotely_disabled=*/false,
        ExtensionUpdateCheckDataKey::kPotentiallyUWS);
  }
  const base::Value* pv_value = attributes.FindKey("_policy_violation");
  if (pv_value != nullptr && pv_value->GetBool()) {
    ReportExtensionDisabledRemotely(
        /*should_be_remotely_disabled=*/false,
        ExtensionUpdateCheckDataKey::kPolicyViolation);
  }
}

}  // namespace extensions
