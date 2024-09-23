// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_allowlist.h"

#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_metrics_collector_factory.h"
#include "components/safe_browsing/core/browser/safe_browsing_metrics_collector.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "extensions/browser/allowlist_state.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"

namespace extensions {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ExtensionAllowlistOmahaAttributeValue {
  kUndefined = 0,
  kAllowlisted = 1,
  kNotAllowlisted = 2,
  kMaxValue = kNotAllowlisted,
};

void ReportExtensionAllowlistOmahaAttribute(
    const base::Value* allowlist_value) {
  ExtensionAllowlistOmahaAttributeValue value;

  if (!allowlist_value)
    value = ExtensionAllowlistOmahaAttributeValue::kUndefined;
  else if (allowlist_value->GetBool())
    value = ExtensionAllowlistOmahaAttributeValue::kAllowlisted;
  else
    value = ExtensionAllowlistOmahaAttributeValue::kNotAllowlisted;

  base::UmaHistogramEnumeration("Extensions.EsbAllowlistOmahaAttribute", value);
}

// Indicates whether an extension is included in the Safe Browsing allowlist.
constexpr PrefMap kPrefAllowlist = {"allowlist", PrefType::kInteger,
                                    PrefScope::kExtensionSpecific};

// Indicates the enforcement acknowledge state for the Safe Browsing allowlist.
constexpr PrefMap kPrefAllowlistAcknowledge = {
    "allowlist_acknowledge", PrefType::kInteger, PrefScope::kExtensionSpecific};

}  // namespace

ExtensionAllowlist::ExtensionAllowlist(Profile* profile,
                                       ExtensionPrefs* extension_prefs,
                                       ExtensionService* extension_service)
    : profile_(profile),
      extension_prefs_(extension_prefs),
      extension_service_(extension_service),
      registry_(ExtensionRegistry::Get(profile)) {
  SetAllowlistEnforcementFields();

  // Relies on ExtensionSystem dependency on ExtensionPrefs to ensure
  // extension_prefs outlives this object.
  extension_prefs_observation_.Observe(extension_prefs);

  // Register to Enhanced Safe Browsing setting changes for allowlist
  // enforcements.
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kSafeBrowsingEnhanced,
      base::BindRepeating(&ExtensionAllowlist::OnSafeBrowsingEnhancedChanged,
                          base::Unretained(this)));
}

ExtensionAllowlist::~ExtensionAllowlist() = default;

void ExtensionAllowlist::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ExtensionAllowlist::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ExtensionAllowlist::Init() {
  if (should_auto_disable_extensions_) {
    ActivateAllowlistEnforcement();
  } else {
    DeactivateAllowlistEnforcement();
  }

  init_done_ = true;
}

AllowlistState ExtensionAllowlist::GetExtensionAllowlistState(
    const ExtensionId& extension_id) const {
  int value = 0;
  if (!extension_prefs_->ReadPrefAsInteger(extension_id, kPrefAllowlist,
                                           &value)) {
    return ALLOWLIST_UNDEFINED;
  }

  if (value < 0 || value > ALLOWLIST_LAST) {
    LOG(ERROR) << "Bad pref 'allowlist' for extension '" << extension_id << "'";
    return ALLOWLIST_UNDEFINED;
  }

  return static_cast<AllowlistState>(value);
}

void ExtensionAllowlist::SetExtensionAllowlistState(
    const ExtensionId& extension_id,
    AllowlistState state) {
  DCHECK_NE(state, ALLOWLIST_UNDEFINED);

  if (state != GetExtensionAllowlistState(extension_id))
    extension_prefs_->SetIntegerPref(extension_id, kPrefAllowlist, state);

  if (warnings_enabled_) {
    NotifyExtensionAllowlistWarningStateChanged(
        extension_id, /*show_warning=*/state == ALLOWLIST_NOT_ALLOWLISTED);
  }
}

AllowlistAcknowledgeState
ExtensionAllowlist::GetExtensionAllowlistAcknowledgeState(
    const ExtensionId& extension_id) const {
  int value = 0;
  if (!extension_prefs_->ReadPrefAsInteger(extension_id,
                                           kPrefAllowlistAcknowledge, &value)) {
    return ALLOWLIST_ACKNOWLEDGE_NONE;
  }

  if (value < 0 || value > ALLOWLIST_ACKNOWLEDGE_LAST) {
    LOG(ERROR) << "Bad pref 'allowlist_acknowledge' for extension '"
               << extension_id << "'";
    return ALLOWLIST_ACKNOWLEDGE_NONE;
  }

  return static_cast<AllowlistAcknowledgeState>(value);
}

void ExtensionAllowlist::SetExtensionAllowlistAcknowledgeState(
    const ExtensionId& extension_id,
    AllowlistAcknowledgeState state) {
  if (state != GetExtensionAllowlistAcknowledgeState(extension_id)) {
    extension_prefs_->SetIntegerPref(extension_id, kPrefAllowlistAcknowledge,
                                     state);
  }
}

void ExtensionAllowlist::PerformActionBasedOnOmahaAttributes(
    const ExtensionId& extension_id,
    const base::Value::Dict& attributes) {
  const base::Value* allowlist_value = attributes.Find("_esbAllowlist");

  ReportExtensionAllowlistOmahaAttribute(allowlist_value);

  if (!allowlist_value) {
    // Ignore missing attribute. Omaha server should set the attribute to |true|
    // or |false|. This way the allowlist state won't flip if there is a server
    // bug where the attribute isn't sent. This will also leave external
    // extensions in the |ALLOWLIST_UNDEFINED| state.
    return;
  }

  AllowlistState allowlist_state = allowlist_value->GetBool()
                                       ? ALLOWLIST_ALLOWLISTED
                                       : ALLOWLIST_NOT_ALLOWLISTED;

  if (allowlist_state == GetExtensionAllowlistState(extension_id)) {
    // Do nothing if the state didn't change.
    return;
  }

  // Set the allowlist state even if there is no enforcement. This will allow
  // immediate enforcement when it is activated.
  SetExtensionAllowlistState(extension_id, allowlist_state);

  if (should_auto_disable_extensions_) {
    if (allowlist_state == ALLOWLIST_ALLOWLISTED) {
      // The extension is now allowlisted, remove the disable reason if present
      // and ask for a user acknowledge if the extension was re-enabled in the
      // process.

      if (!extension_prefs_->HasDisableReason(
              extension_id, disable_reason::DISABLE_NOT_ALLOWLISTED)) {
        // Nothing to do if the extension was not already disabled by allowlist
        // enforcement.
        return;
      }

      extension_service_->RemoveDisableReasonAndMaybeEnable(
          extension_id, disable_reason::DISABLE_NOT_ALLOWLISTED);

      if (registry_->enabled_extensions().Contains(extension_id)) {
        // Inform the user if the extension is now enabled.
        SetExtensionAllowlistAcknowledgeState(extension_id,
                                              ALLOWLIST_ACKNOWLEDGE_NEEDED);
      }
    } else {
      // The extension is no longer allowlisted, try to apply enforcement.
      ApplyEnforcement(extension_id);
    }
  }
}

bool ExtensionAllowlist::ShouldDisplayWarning(
    const ExtensionId& extension_id) const {
  if (!warnings_enabled_)
    return false;  // No warnings should be shown.

  // Do not display warnings for extensions explicitly allowed by policy
  // (forced, recommenced and allowed extensions).
  // TODO(jeffcyr): Policy allowed extensions should also be exempted from auto
  // disable.
  ExtensionManagement* settings =
      ExtensionManagementFactory::GetForBrowserContext(profile_);
  if (settings->IsInstallationExplicitlyAllowed(extension_id))
    return false;  // Extension explicitly allowed.

  if (GetExtensionAllowlistState(extension_id) != ALLOWLIST_NOT_ALLOWLISTED)
    return false;  // Extension is allowlisted.

  // Warn about the extension.
  return true;
}

void ExtensionAllowlist::OnExtensionInstalled(const ExtensionId& extension_id,
                                              int install_flags) {
  // Check if a user clicked through the install friction and set the
  // acknowledge state accordingly.
  if (install_flags & kInstallFlagBypassedSafeBrowsingFriction) {
    SetExtensionAllowlistAcknowledgeState(
        extension_id, ALLOWLIST_ACKNOWLEDGE_ENABLED_BY_USER);
    SetExtensionAllowlistState(extension_id, ALLOWLIST_NOT_ALLOWLISTED);
  }
}

void ExtensionAllowlist::SetAllowlistEnforcementFields() {
  if (safe_browsing::IsEnhancedProtectionEnabled(*profile_->GetPrefs())) {
    warnings_enabled_ = base::FeatureList::IsEnabled(
        extensions_features::kSafeBrowsingCrxAllowlistShowWarnings);
    should_auto_disable_extensions_ = base::FeatureList::IsEnabled(
        extensions_features::kSafeBrowsingCrxAllowlistAutoDisable);
  } else {
    warnings_enabled_ = false;
    should_auto_disable_extensions_ = false;
  }
}

// `ApplyEnforcement` can be called when an extension becomes not allowlisted or
// when the allowlist enforcement is activated (for already not allowlisted
// extensions).
void ExtensionAllowlist::ApplyEnforcement(const ExtensionId& extension_id) {
  DCHECK(should_auto_disable_extensions_);
  DCHECK_EQ(GetExtensionAllowlistState(extension_id),
            ALLOWLIST_NOT_ALLOWLISTED);

  // Early exit if the enforcement is already done.
  if (extension_prefs_->HasDisableReason(
          extension_id, disable_reason::DISABLE_NOT_ALLOWLISTED)) {
    return;
  }

  // Do not re-enforce if the extension was explicitly enabled by the user.
  if (GetExtensionAllowlistAcknowledgeState(extension_id) ==
      ALLOWLIST_ACKNOWLEDGE_ENABLED_BY_USER) {
    return;
  }

  bool was_enabled = registry_->enabled_extensions().Contains(extension_id);
  extension_service_->DisableExtension(extension_id,
                                       disable_reason::DISABLE_NOT_ALLOWLISTED);

  // The user should acknowledge the disable action if the extension was
  // previously enabled and the disable reason could be added (it can be denied
  // by policy).
  if (was_enabled &&
      extension_prefs_->HasDisableReason(
          extension_id, disable_reason::DISABLE_NOT_ALLOWLISTED)) {
    SetExtensionAllowlistAcknowledgeState(extension_id,
                                          ALLOWLIST_ACKNOWLEDGE_NEEDED);
  } else {
    SetExtensionAllowlistAcknowledgeState(extension_id,
                                          ALLOWLIST_ACKNOWLEDGE_NONE);
  }
}

void ExtensionAllowlist::ActivateAllowlistEnforcement() {
  DCHECK(should_auto_disable_extensions_);

  const ExtensionSet all_extensions =
      registry_->GenerateInstalledExtensionsSet();
  for (const auto& extension : all_extensions) {
    if (GetExtensionAllowlistState(extension->id()) ==
        ALLOWLIST_NOT_ALLOWLISTED) {
      ApplyEnforcement(extension->id());
    }
  }
}

void ExtensionAllowlist::DeactivateAllowlistEnforcement() {
  DCHECK(!should_auto_disable_extensions_);

  const ExtensionSet all_extensions =
      registry_->GenerateInstalledExtensionsSet();

  // Find all extensions disabled by allowlist enforcement, remove the disable
  // reason and reset the acknowledge state.
  for (const auto& extension : all_extensions) {
    if (extension_prefs_->HasDisableReason(
            extension->id(), disable_reason::DISABLE_NOT_ALLOWLISTED)) {
      extension_service_->RemoveDisableReasonAndMaybeEnable(
          extension->id(), disable_reason::DISABLE_NOT_ALLOWLISTED);
      SetExtensionAllowlistAcknowledgeState(extension->id(),
                                            ALLOWLIST_ACKNOWLEDGE_NONE);
    }
  }
}

void ExtensionAllowlist::OnSafeBrowsingEnhancedChanged() {
  bool previous_auto_disable = should_auto_disable_extensions_;
  bool previous_warnings_enabled = warnings_enabled_;

  // Note that `should_auto_disable_extensions_` could remain `false` even if
  // the ESB setting was turned on if the feature flag is disabled.
  SetAllowlistEnforcementFields();

  if (previous_auto_disable != should_auto_disable_extensions_) {
    if (should_auto_disable_extensions_) {
      ActivateAllowlistEnforcement();
    } else {
      DeactivateAllowlistEnforcement();
    }
  }

  if (previous_warnings_enabled != warnings_enabled_) {
    const ExtensionSet all_extensions =
        registry_->GenerateInstalledExtensionsSet();

    for (const auto& extension : all_extensions) {
      if (GetExtensionAllowlistState(extension->id()) ==
          ALLOWLIST_NOT_ALLOWLISTED) {
        NotifyExtensionAllowlistWarningStateChanged(
            extension->id(), /*show_warning=*/warnings_enabled_);
      }
    }
  }
}

// ExtensionPrefsObserver::OnExtensionStateChanged override
void ExtensionAllowlist::OnExtensionStateChanged(
    const ExtensionId& extension_id,
    bool is_now_enabled) {
  // TODO(crbug.com/40757123): Can be removed when the bug is resolved. This
  // check is needed because `OnExtensionStateChanged` is called for all loaded
  // extensions during startup. So on the first startup with the enforcement
  // enabled, all not allowlisted extensions would be
  // `ALLOWLIST_ACKNOWLEDGE_ENABLED_BY_USER` instead of disabled.
  if (!init_done_)
    return;

  if (!is_now_enabled)
    return;  // We only care if the extension is now enabled.

  if (!should_auto_disable_extensions_)
    return;  // We only care if allowlist if being enforced.

  if (GetExtensionAllowlistState(extension_id) != ALLOWLIST_NOT_ALLOWLISTED) {
    // We only care if the current state is not allowlisted.
    return;
  }

  if (GetExtensionAllowlistAcknowledgeState(extension_id) ==
      ALLOWLIST_ACKNOWLEDGE_ENABLED_BY_USER) {
    // The extension was already enabled and acknowledged by the user.
    return;
  }

  // The extension was enabled even though it's not on the allowlist. Consider
  // this an acknowledgement from the user, and ensure we don't disable the
  // extension again.
  ReportExtensionReEnabledEvent();
  SetExtensionAllowlistAcknowledgeState(extension_id,
                                        ALLOWLIST_ACKNOWLEDGE_ENABLED_BY_USER);
}

void ExtensionAllowlist::NotifyExtensionAllowlistWarningStateChanged(
    const ExtensionId& extension_id,
    bool show_warning) {
  for (auto& observer : observers_) {
    observer.OnExtensionAllowlistWarningStateChanged(extension_id,
                                                     show_warning);
  }
}

void ExtensionAllowlist::ReportExtensionReEnabledEvent() {
  auto* metrics_collector =
      safe_browsing::SafeBrowsingMetricsCollectorFactory::GetForProfile(
          profile_);
  DCHECK(metrics_collector);
  if (metrics_collector) {
    metrics_collector->AddSafeBrowsingEventToPref(
        safe_browsing::SafeBrowsingMetricsCollector::EventType::
            NON_ALLOWLISTED_EXTENSION_RE_ENABLED);
  }
}

}  // namespace extensions
