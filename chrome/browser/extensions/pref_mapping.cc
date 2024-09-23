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
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/embedder_support/pref_names.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/translate/core/browser/translate_pref_names.h"

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/chromeos/extensions/controlled_pref_mapping.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/containers/fixed_flat_map.h"
#endif

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
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
    {"autoclick", chromeos::prefs::kAccessibilityAutoclickEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"caretHighlight", chromeos::prefs::kAccessibilityCaretHighlightEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"cursorColor", chromeos::prefs::kAccessibilityCursorColorEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"cursorHighlight", chromeos::prefs::kAccessibilityCursorHighlightEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"dictation", chromeos::prefs::kAccessibilityDictationEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"dockedMagnifier", chromeos::prefs::kDockedMagnifierEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"focusHighlight", chromeos::prefs::kAccessibilityFocusHighlightEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"highContrast", chromeos::prefs::kAccessibilityHighContrastEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"largeCursor", chromeos::prefs::kAccessibilityLargeCursorEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"screenMagnifier", chromeos::prefs::kAccessibilityScreenMagnifierEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"selectToSpeak", chromeos::prefs::kAccessibilitySelectToSpeakEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"spokenFeedback", chromeos::prefs::kAccessibilitySpokenFeedbackEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"stickyKeys", chromeos::prefs::kAccessibilityStickyKeysEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"switchAccess", chromeos::prefs::kAccessibilitySwitchAccessEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"virtualKeyboard", chromeos::prefs::kAccessibilityVirtualKeyboardEnabled,
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

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// Given a pref name for an extension-controlled pref where the underlying
// pref is controlled by ash, returns the PrefPath used by the crosapi to set
// the pref in ash, or nullptr if no pref exists.
crosapi::mojom::PrefPath PrefMapping::GetPrefPathForPrefName(
    const std::string& pref_name) const {
  static constexpr auto name_to_extension_prefpath = base::MakeFixedFlatMap<
      std::string_view, crosapi::mojom::PrefPath>(
      {{chromeos::prefs::kDockedMagnifierEnabled,
        crosapi::mojom::PrefPath::kDockedMagnifierEnabled},
       {chromeos::prefs::kAccessibilityAutoclickEnabled,
        crosapi::mojom::PrefPath::kAccessibilityAutoclickEnabled},
       {chromeos::prefs::kAccessibilityCaretHighlightEnabled,
        crosapi::mojom::PrefPath::kAccessibilityCaretHighlightEnabled},
       {chromeos::prefs::kAccessibilityCursorColorEnabled,
        crosapi::mojom::PrefPath::kAccessibilityCursorColorEnabled},
       {chromeos::prefs::kAccessibilityCursorHighlightEnabled,
        crosapi::mojom::PrefPath::kAccessibilityCursorHighlightEnabled},
       {chromeos::prefs::kAccessibilityDictationEnabled,
        crosapi::mojom::PrefPath::kAccessibilityDictationEnabled},
       {chromeos::prefs::kAccessibilityFocusHighlightEnabled,
        crosapi::mojom::PrefPath::kAccessibilityFocusHighlightEnabled},
       {chromeos::prefs::kAccessibilityHighContrastEnabled,
        crosapi::mojom::PrefPath::kAccessibilityHighContrastEnabled},
       {chromeos::prefs::kAccessibilityLargeCursorEnabled,
        crosapi::mojom::PrefPath::kAccessibilityLargeCursorEnabled},
       {chromeos::prefs::kAccessibilityScreenMagnifierEnabled,
        crosapi::mojom::PrefPath::kAccessibilityScreenMagnifierEnabled},
       {chromeos::prefs::kAccessibilitySelectToSpeakEnabled,
        crosapi::mojom::PrefPath::kAccessibilitySelectToSpeakEnabled},
       {chromeos::prefs::kAccessibilitySpokenFeedbackEnabled,
        crosapi::mojom::PrefPath::kExtensionAccessibilitySpokenFeedbackEnabled},
       {chromeos::prefs::kAccessibilityStickyKeysEnabled,
        crosapi::mojom::PrefPath::kAccessibilityStickyKeysEnabled},
       {chromeos::prefs::kAccessibilitySwitchAccessEnabled,
        crosapi::mojom::PrefPath::kAccessibilitySwitchAccessEnabled},
       {chromeos::prefs::kAccessibilityVirtualKeyboardEnabled,
        crosapi::mojom::PrefPath::kAccessibilityVirtualKeyboardEnabled},
       {proxy_config::prefs::kProxy, crosapi::mojom::PrefPath::kProxy}});
  auto pref_iter = name_to_extension_prefpath.find(pref_name);
  return pref_iter == name_to_extension_prefpath.end()
             ? crosapi::mojom::PrefPath::kUnknown
             : pref_iter->second;
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

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
