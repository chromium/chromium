// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/omaha_attributes_handler.h"

#include <optional>

#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/blocklist_state.h"

namespace extensions {

namespace {

// Logs UMA metrics when an extension is disabled remotely.
void ReportExtensionDisabledRemotely(bool should_be_remotely_disabled,
                                     ExtensionUpdateCheckDataKey reason) {
  // Report that the extension is newly disabled due to Omaha attributes.
  if (should_be_remotely_disabled) {
    base::UmaHistogramEnumeration("Extensions.ExtensionDisabledRemotely2",
                                  reason);
  }

  // Report that the extension has added a new disable reason.
  base::UmaHistogramEnumeration(
      "Extensions.ExtensionAddDisabledRemotelyReason2", reason);
}

// Logs UMA metrics when the key is not found in Omaha attributes.
void ReportNoUpdateCheckKeys() {
  base::UmaHistogramEnumeration("Extensions.ExtensionDisabledRemotely2",
                                ExtensionUpdateCheckDataKey::kNoKey);
}

// Logs UMA metrics when a remotely disabled extension is re-enabled.
void ReportReenableExtension(ExtensionUpdateCheckDataKey reason) {
  const char* histogram = nullptr;
  switch (reason) {
    case ExtensionUpdateCheckDataKey::kMalware:
      histogram = "Extensions.ExtensionReenabledRemotely";
      break;
    case ExtensionUpdateCheckDataKey::kPotentiallyUWS:
      histogram = "Extensions.ExtensionReenabledRemotelyForPotentiallyUWS";
      break;
    case ExtensionUpdateCheckDataKey::kPolicyViolation:
      histogram = "Extensions.ExtensionReenabledRemotelyForPolicyViolation";
      break;
    case ExtensionUpdateCheckDataKey::kNoKey:
      NOTREACHED_IN_MIGRATION();
  }
  base::UmaHistogramCounts100(histogram, 1);
}

// Checks whether the `state` is in the `attributes`.
bool HasOmahaBlocklistStateInAttributes(const base::Value::Dict& attributes,
                                        BitMapBlocklistState state) {
  std::optional<bool> state_value;
  switch (state) {
    case BitMapBlocklistState::BLOCKLISTED_MALWARE:
      state_value = attributes.FindBool("_malware");
      break;
    case BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION:
      state_value = attributes.FindBool("_policy_violation");
      break;
    case BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED:
      state_value = attributes.FindBool("_potentially_uws");
      break;
    case BitMapBlocklistState::NOT_BLOCKLISTED:
    case BitMapBlocklistState::BLOCKLISTED_SECURITY_VULNERABILITY:
      NOTREACHED_IN_MIGRATION()
          << "The other states are not applicable in Omaha attributes.";
      state_value = std::nullopt;
      break;
  }
  return state_value.value_or(false);
}

}  // namespace

OmahaAttributesHandler::OmahaAttributesHandler(
    ExtensionPrefs* extension_prefs,
    ExtensionRegistry* registry,
    ExtensionService* extension_service)
    : extension_prefs_(extension_prefs),
      registry_(registry),
      extension_service_(extension_service) {}

void OmahaAttributesHandler::PerformActionBasedOnOmahaAttributes(
    const ExtensionId& extension_id,
    const base::Value::Dict& attributes) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // It is possible that an extension is uninstalled when the omaha attributes
  // are notified by the update client asynchronously. In this case, we should
  // ignore this extension.
  if (!registry_->GetInstalledExtension(extension_id)) {
    return;
  }
  HandleMalwareOmahaAttribute(extension_id, attributes);
  HandleGreylistOmahaAttribute(
      extension_id, attributes,
      BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
      ExtensionUpdateCheckDataKey::kPolicyViolation);
  HandleGreylistOmahaAttribute(
      extension_id, attributes,
      BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED,
      ExtensionUpdateCheckDataKey::kPotentiallyUWS);
}

void OmahaAttributesHandler::HandleMalwareOmahaAttribute(
    const ExtensionId& extension_id,
    const base::Value::Dict& attributes) {
  bool has_malware_value = HasOmahaBlocklistStateInAttributes(
      attributes, BitMapBlocklistState::BLOCKLISTED_MALWARE);
  if (!has_malware_value) {
    ReportNoUpdateCheckKeys();
    if (!blocklist_prefs::HasOmahaBlocklistState(
            extension_id, BitMapBlocklistState::BLOCKLISTED_MALWARE,
            extension_prefs_)) {
      return;
    }
    // The extension was previously blocklisted by Omaha, but is no longer.
    // Clear the old omaha state.
    ReportReenableExtension(ExtensionUpdateCheckDataKey::kMalware);
    blocklist_prefs::RemoveOmahaBlocklistState(
        extension_id, BitMapBlocklistState::BLOCKLISTED_MALWARE,
        extension_prefs_);
    extension_service_->OnBlocklistStateRemoved(extension_id);
    return;
  }

  if (blocklist_prefs::HasOmahaBlocklistState(
          extension_id, BitMapBlocklistState::BLOCKLISTED_MALWARE,
          extension_prefs_)) {
    // The extension is already disabled. No work needs to be done.
    return;
  }

  ReportExtensionDisabledRemotely(
      extension_service_->IsExtensionEnabled(extension_id),
      ExtensionUpdateCheckDataKey::kMalware);

  blocklist_prefs::AddOmahaBlocklistState(
      extension_id, BitMapBlocklistState::BLOCKLISTED_MALWARE,
      extension_prefs_);
  extension_service_->OnBlocklistStateAdded(extension_id);
}

void OmahaAttributesHandler::HandleGreylistOmahaAttribute(
    const ExtensionId& extension_id,
    const base::Value::Dict& attributes,
    BitMapBlocklistState greylist_state,
    ExtensionUpdateCheckDataKey reason) {
  bool has_attribute_value =
      HasOmahaBlocklistStateInAttributes(attributes, greylist_state);
  bool has_omaha_blocklist_state = blocklist_prefs::HasOmahaBlocklistState(
      extension_id, greylist_state, extension_prefs_);
  if (!has_attribute_value) {
    if (has_omaha_blocklist_state) {
      blocklist_prefs::RemoveOmahaBlocklistState(extension_id, greylist_state,
                                                 extension_prefs_);
      ReportReenableExtension(reason);
    }
    extension_service_->OnGreylistStateRemoved(extension_id);
    return;
  }

  ReportExtensionDisabledRemotely(
      /*should_be_remotely_disabled=*/!has_omaha_blocklist_state, reason);
  blocklist_prefs::AddOmahaBlocklistState(extension_id, greylist_state,
                                          extension_prefs_);
  extension_service_->OnGreylistStateAdded(extension_id, greylist_state);
}

}  // namespace extensions
