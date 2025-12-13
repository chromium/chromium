// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/pref_mapping.h"

#include <optional>
#include <span>  // std::size.
#include <string_view>

#include "base/containers/contains.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/pref_transformer_interface.h"
#include "chrome/browser/prefetch/pref_names.h"
#include "chrome/browser/preloading/preloading_prefs.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/embedder_support/pref_names.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_pref_names.h"
#endif

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

using extensions::mojom::APIPermissionID;

namespace extensions {

namespace {

constexpr char kOnPrefChangeFormat[] = "types.ChromeSetting.%s.onChange";

const PrefMappingEntry kMappings[] = {
    {"alternateErrorPagesEnabled",
     embedder_support::kAlternateErrorPagesEnabled, APIPermissionID::kPrivacy,
     APIPermissionID::kPrivacy},
    {"autofillEnabled", autofill::prefs::kAutofillEnabledDeprecated,
     APIPermissionID::kPrivacy, APIPermissionID::kPrivacy},
    {"autofillAddressEnabled", autofill::prefs::kAutofillProfileEnabled,
     APIPermissionID::kPrivacy, APIPermissionID::kPrivacy},
    {"autofillCreditCardEnabled", autofill::prefs::kAutofillCreditCardEnabled,
     APIPermissionID::kPrivacy, APIPermissionID::kPrivacy},
    {"hyperlinkAuditingEnabled", prefs::kEnableHyperlinkAuditing,
     APIPermissionID::kPrivacy, APIPermissionID::kPrivacy},
    {"networkPredictionEnabled", prefetch::prefs::kNetworkPredictionOptions,
     APIPermissionID::kPrivacy, APIPermissionID::kPrivacy},
    {"passwordSavingEnabled",
     password_manager::prefs::kCredentialsEnableService,
     APIPermissionID::kPrivacy, APIPermissionID::kPrivacy},

    {"protectedContentEnabled", prefs::kProtectedContentDefault,
     APIPermissionID::kPrivacy, APIPermissionID::kPrivacy},

    {"proxy", proxy_config::prefs::kProxy, APIPermissionID::kProxy,
     APIPermissionID::kProxy},
    {"referrersEnabled", prefs::kEnableReferrers, APIPermissionID::kPrivacy,
     APIPermissionID::kPrivacy},
    {"doNotTrackEnabled", prefs::kEnableDoNotTrack, APIPermissionID::kPrivacy,
     APIPermissionID::kPrivacy},
    {"safeBrowsingEnabled", prefs::kSafeBrowsingEnabled,
     APIPermissionID::kPrivacy, APIPermissionID::kPrivacy},
    {"safeBrowsingExtendedReportingEnabled",
     prefs::kSafeBrowsingScoutReportingEnabled, APIPermissionID::kPrivacy,
     APIPermissionID::kPrivacy},
    {"searchSuggestEnabled", prefs::kSearchSuggestEnabled,
     APIPermissionID::kPrivacy, APIPermissionID::kPrivacy},
    {"spellingServiceEnabled", spellcheck::prefs::kSpellCheckUseSpellingService,
     APIPermissionID::kPrivacy, APIPermissionID::kPrivacy},
    {"thirdPartyCookiesAllowed", prefs::kCookieControlsMode,
     APIPermissionID::kPrivacy, APIPermissionID::kPrivacy},
    {"topicsEnabled", prefs::kPrivacySandboxM1TopicsEnabled,
     APIPermissionID::kPrivacy, APIPermissionID::kPrivacy},
    {"fledgeEnabled", prefs::kPrivacySandboxM1FledgeEnabled,
     APIPermissionID::kPrivacy, APIPermissionID::kPrivacy},
    {"adMeasurementEnabled", prefs::kPrivacySandboxM1AdMeasurementEnabled,
     APIPermissionID::kPrivacy, APIPermissionID::kPrivacy},
    {"translationServiceEnabled", translate::prefs::kOfferTranslateEnabled,
     APIPermissionID::kPrivacy, APIPermissionID::kPrivacy},
    {"webRTCIPHandlingPolicy", prefs::kWebRTCIPHandlingPolicy,
     APIPermissionID::kPrivacy, APIPermissionID::kPrivacy},
    {"webRTCIPHandlingUrl", prefs::kWebRTCIPHandlingUrl,
     APIPermissionID::kPrivacy, APIPermissionID::kPrivacy},
    {"webRTCPostQuantumKeyAgreement", prefs::kWebRTCPostQuantumKeyAgreement,
     APIPermissionID::kPrivacy, APIPermissionID::kPrivacy},
    {"webRTCUDPPortRange", prefs::kWebRTCUDPPortRange,
     APIPermissionID::kPrivacy, APIPermissionID::kPrivacy},
    {"relatedWebsiteSetsEnabled",
     prefs::kPrivacySandboxRelatedWebsiteSetsEnabled, APIPermissionID::kPrivacy,
     APIPermissionID::kPrivacy},
    // accessibilityFeatures.animationPolicy is available for
    // all platforms but the others from accessibilityFeatures
    // is only available for OS_CHROMEOS.
    {"animationPolicy", prefs::kAnimationPolicy,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
// Below is the list of extension-controlled preferences where the underlying
// feature being controlled exists in ash. They should be kept in sync/in order.
// If a new extension-controlled pref of this type is added, it should be added
// to both lists.
#if BUILDFLAG(IS_CHROMEOS)
    {"autoclick", ash::prefs::kAccessibilityAutoclickEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"caretHighlight", ash::prefs::kAccessibilityCaretHighlightEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"cursorColor", ash::prefs::kAccessibilityCursorColorEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"cursorHighlight", ash::prefs::kAccessibilityCursorHighlightEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"dictation", ash::prefs::kAccessibilityDictationEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"dockedMagnifier", ash::prefs::kDockedMagnifierEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"focusHighlight", ash::prefs::kAccessibilityFocusHighlightEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"highContrast", ash::prefs::kAccessibilityHighContrastEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"largeCursor", ash::prefs::kAccessibilityLargeCursorEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"screenMagnifier", ash::prefs::kAccessibilityScreenMagnifierEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"selectToSpeak", ash::prefs::kAccessibilitySelectToSpeakEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"spokenFeedback", ash::prefs::kAccessibilitySpokenFeedbackEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"stickyKeys", ash::prefs::kAccessibilityStickyKeysEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"switchAccess", ash::prefs::kAccessibilitySwitchAccessEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"virtualKeyboard", ash::prefs::kAccessibilityVirtualKeyboardEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
#endif
};

class IdentityPrefTransformer : public PrefTransformerInterface {
 public:
  std::optional<base::Value> ExtensionToBrowserPref(
      const base::Value& extension_pref,
      std::string& error,
      bool& bad_message) override {
    return extension_pref.Clone();
  }

  std::optional<base::Value> BrowserToExtensionPref(
      const base::Value& browser_pref,
      bool is_incognito_profile) override {
    return browser_pref.Clone();
  }
};

}  // namespace

// static
PrefMapping* PrefMapping::GetInstance() {
  return base::Singleton<PrefMapping>::get();
}

// static
base::span<const PrefMappingEntry> PrefMapping::GetMappings() {
  return kMappings;
}

bool PrefMapping::FindBrowserPrefForExtensionPref(
    const std::string& extension_pref,
    std::string* browser_pref,
    APIPermissionID* read_permission,
    APIPermissionID* write_permission) const {
  auto it = mapping_.find(extension_pref);
  if (it == mapping_.end())
    return false;
  *browser_pref = it->second.pref_name;
  *read_permission = it->second.read_permission;
  *write_permission = it->second.write_permission;
  return true;
}

bool PrefMapping::FindEventForBrowserPref(const std::string& browser_pref,
                                          std::string* event_name,
                                          APIPermissionID* permission) const {
  auto it = event_mapping_.find(browser_pref);
  if (it == event_mapping_.end())
    return false;
  *event_name = it->second.pref_name;
  *permission = it->second.read_permission;
  return true;
}

PrefTransformerInterface* PrefMapping::FindTransformerForBrowserPref(
    const std::string& browser_pref) const {
  auto it = transformers_.find(browser_pref);
  if (it != transformers_.end())
    return it->second.get();
  return identity_transformer_.get();
}

PrefMapping::PrefMapping() {
  identity_transformer_ = std::make_unique<IdentityPrefTransformer>();
  for (const auto& pref : kMappings) {
    mapping_[pref.extension_pref] = PrefMapData(
        pref.browser_pref, pref.read_permission, pref.write_permission);
    std::string event_name =
        base::StringPrintf(kOnPrefChangeFormat, pref.extension_pref);
    event_mapping_[pref.browser_pref] =
        PrefMapData(event_name, pref.read_permission, pref.write_permission);
  }
  DCHECK_EQ(std::size(kMappings), mapping_.size());
  DCHECK_EQ(std::size(kMappings), event_mapping_.size());
}

PrefMapping::~PrefMapping() = default;

void PrefMapping::RegisterPrefTransformer(
    const std::string& browser_pref,
    std::unique_ptr<PrefTransformerInterface> transformer) {
  DCHECK(!base::Contains(transformers_, browser_pref))
      << "Trying to register pref transformer for " << browser_pref << " twice";
  transformers_[browser_pref] = std::move(transformer);
}

}  // namespace extensions
