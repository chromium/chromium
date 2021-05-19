// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/omaha_attributes_handler.h"

#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "chrome/browser/extensions/blocklist_extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/blocklist_state.h"
#include "extensions/common/extension_features.h"

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

// static
bool OmahaAttributesHandler::HasOmahaBlocklistStateInAttributes(
    const base::Value& attributes,
    BitMapBlocklistState state) {
  const base::Value* state_value = nullptr;
  switch (state) {
    case BitMapBlocklistState::BLOCKLISTED_MALWARE:
      state_value = attributes.FindKey("_malware");
      break;
    case BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION:
      state_value = attributes.FindKey("_policy_violation");
      break;
    case BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED:
      state_value = attributes.FindKey("_potentially_uws");
      break;
    case BitMapBlocklistState::NOT_BLOCKLISTED:
    case BitMapBlocklistState::BLOCKLISTED_SECURITY_VULNERABILITY:
      NOTREACHED()
          << "The other states are not applicable in Omaha attributes.";
      state_value = nullptr;
      break;
  }
  return state_value && state_value->GetBool();
}

OmahaAttributesHandler::OmahaAttributesHandler(
    ExtensionPrefs* extension_prefs,
    ExtensionService* extension_service)
    : extension_prefs_(extension_prefs),
      extension_service_(extension_service) {}

void OmahaAttributesHandler::PerformActionBasedOnOmahaAttributes(
    const ExtensionId& extension_id,
    const base::Value& attributes) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ReportPolicyViolationUWSOmahaAttributes(attributes);
  HandleGreylistOmahaAttribute(
      extension_id, attributes,
      extensions_features::kDisablePolicyViolationExtensionsRemotely,
      BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION);
  HandleGreylistOmahaAttribute(
      extension_id, attributes,
      extensions_features::kDisablePotentiallyUwsExtensionsRemotely,
      BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED);
}

void OmahaAttributesHandler::ReportPolicyViolationUWSOmahaAttributes(
    const base::Value& attributes) {
  bool has_uws_value = HasOmahaBlocklistStateInAttributes(
      attributes, BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED);
  if (has_uws_value) {
    ReportExtensionDisabledRemotely(
        /*should_be_remotely_disabled=*/false,
        ExtensionUpdateCheckDataKey::kPotentiallyUWS);
  }
  bool has_pv_value = HasOmahaBlocklistStateInAttributes(
      attributes, BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION);
  if (has_pv_value) {
    ReportExtensionDisabledRemotely(
        /*should_be_remotely_disabled=*/false,
        ExtensionUpdateCheckDataKey::kPolicyViolation);
  }
}

void OmahaAttributesHandler::HandleGreylistOmahaAttribute(
    const ExtensionId& extension_id,
    const base::Value& attributes,
    const base::Feature& feature_flag,
    BitMapBlocklistState greylist_state) {
  bool has_attribute_value =
      HasOmahaBlocklistStateInAttributes(attributes, greylist_state);
  if (!base::FeatureList::IsEnabled(feature_flag) || !has_attribute_value) {
    blocklist_prefs::RemoveOmahaBlocklistState(extension_id, greylist_state,
                                               extension_prefs_);
    extension_service_->ClearGreylistedAcknowledgedStateAndMaybeReenable(
        extension_id);
    return;
  }

  blocklist_prefs::AddOmahaBlocklistState(extension_id, greylist_state,
                                          extension_prefs_);
  extension_service_->MaybeDisableGreylistedExtension(extension_id,
                                                      greylist_state);
}

}  // namespace extensions
