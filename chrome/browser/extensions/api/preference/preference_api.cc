// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/preference/preference_api.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/lazy_instance.h"
#include "base/memory/singleton.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/preference/preference_helpers.h"
#include "chrome/browser/extensions/api/proxy/proxy_api.h"
#include "chrome/browser/extensions/api/system_indicator/system_indicator_api.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/prefetch/pref_names.h"
#include "chrome/browser/prefetch/prefetch_prefs.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/embedder_support/pref_names.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "extensions/browser/api/content_settings/content_settings_service.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_pref_names.h"  // nogncheck
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/chromeos/extensions/controlled_pref_mapping.h"
#include "chromeos/startup/browser_params_proxy.h"
#endif

using extensions::mojom::APIPermissionID;

namespace extensions {

namespace {

struct PrefMappingEntry {
  // Name of the preference referenced by the extension API JSON.
  const char* extension_pref;

  // Name of the preference in the PrefStores.
  const char* browser_pref;

  // Permission required to read and observe this preference.
  // Use APIPermissionID::kInvalid for |read_permission| to express that
  // the read permission should not be granted.
  APIPermissionID read_permission;

  // Permission required to write this preference.
  // Use APIPermissionID::kInvalid for |write_permission| to express that
  // the write permission should not be granted.
  APIPermissionID write_permission;
};

constexpr char kOnPrefChangeFormat[] = "types.ChromeSetting.%s.onChange";
constexpr char kConversionErrorMessage[] =
    "Internal error: Stored value for preference '*' cannot be converted "
    "properly.";
constexpr char kPermissionErrorMessage[] =
    "You do not have permission to access the preference '*'. "
    "Be sure to declare in your manifest what permissions you need.";
#if BUILDFLAG(IS_CHROMEOS_LACROS)
constexpr char kPrimaryProfileOnlyErrorMessage[] =
    "You may only access the preference '*' in the primary profile.";
#endif
constexpr char kIncognitoKey[] = "incognito";
constexpr char kScopeKey[] = "scope";
constexpr char kIncognitoSpecific[] = "incognitoSpecific";
constexpr char kLevelOfControl[] = "levelOfControl";
constexpr char kValue[] = "value";

const PrefMappingEntry kPrefMapping[] = {
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

    // Note in Lacros this is Ash-controlled.
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
    {"privacySandboxEnabled", prefs::kPrivacySandboxApisEnabled,
     APIPermissionID::kPrivacy, APIPermissionID::kPrivacy},
    {"translationServiceEnabled", translate::prefs::kOfferTranslateEnabled,
     APIPermissionID::kPrivacy, APIPermissionID::kPrivacy},
    {"webRTCIPHandlingPolicy", prefs::kWebRTCIPHandlingPolicy,
     APIPermissionID::kPrivacy, APIPermissionID::kPrivacy},
    {"webRTCUDPPortRange", prefs::kWebRTCUDPPortRange,
     APIPermissionID::kPrivacy, APIPermissionID::kPrivacy},
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
  std::unique_ptr<base::Value> ExtensionToBrowserPref(
      const base::Value* extension_pref,
      std::string* error,
      bool* bad_message) override {
    return base::Value::ToUniquePtrValue(extension_pref->Clone());
  }

  std::unique_ptr<base::Value> BrowserToExtensionPref(
      const base::Value* browser_pref,
      bool is_incognito_profile) override {
    return base::Value::ToUniquePtrValue(browser_pref->Clone());
  }
};

// Transform the thirdPartyCookiesAllowed extension api to CookieControlsMode
// enum values.
class CookieControlsModeTransformer : public PrefTransformerInterface {
  using CookieControlsMode = content_settings::CookieControlsMode;

 public:
  std::unique_ptr<base::Value> ExtensionToBrowserPref(
      const base::Value* extension_pref,
      std::string* error,
      bool* bad_message) override {
    bool third_party_cookies_allowed = extension_pref->GetBool();
    return std::make_unique<base::Value>(static_cast<int>(
        third_party_cookies_allowed ? CookieControlsMode::kOff
                                    : CookieControlsMode::kBlockThirdParty));
  }

  std::unique_ptr<base::Value> BrowserToExtensionPref(
      const base::Value* browser_pref,
      bool is_incognito_profile) override {
    auto cookie_control_mode =
        static_cast<CookieControlsMode>(browser_pref->GetInt());

    bool third_party_cookies_allowed =
        cookie_control_mode == content_settings::CookieControlsMode::kOff ||
        (!is_incognito_profile &&
         cookie_control_mode == CookieControlsMode::kIncognitoOnly);

    return std::make_unique<base::Value>(third_party_cookies_allowed);
  }
};

class NetworkPredictionTransformer : public PrefTransformerInterface {
 public:
  std::unique_ptr<base::Value> ExtensionToBrowserPref(
      const base::Value* extension_pref,
      std::string* error,
      bool* bad_message) override {
    if (!extension_pref->is_bool()) {
      DCHECK(false) << "Preference not found.";
    } else if (extension_pref->GetBool()) {
      return std::make_unique<base::Value>(
          static_cast<int>(prefetch::NetworkPredictionOptions::kDefault));
    }
    return std::make_unique<base::Value>(
        static_cast<int>(prefetch::NetworkPredictionOptions::kDisabled));
  }

  std::unique_ptr<base::Value> BrowserToExtensionPref(
      const base::Value* browser_pref,
      bool is_incognito_profile) override {
    prefetch::NetworkPredictionOptions value =
        prefetch::NetworkPredictionOptions::kDefault;
    if (browser_pref->is_int()) {
      value = static_cast<prefetch::NetworkPredictionOptions>(
          browser_pref->GetInt());
    }
    return std::make_unique<base::Value>(
        value != prefetch::NetworkPredictionOptions::kDisabled);
  }
};

class ProtectedContentEnabledTransformer : public PrefTransformerInterface {
 public:
  std::unique_ptr<base::Value> ExtensionToBrowserPref(
      const base::Value* extension_pref,
      std::string* error,
      bool* bad_message) override {
    bool protected_identifier_allowed = extension_pref->GetBool();
    return std::make_unique<base::Value>(
        static_cast<int>(protected_identifier_allowed ? CONTENT_SETTING_ALLOW
                                                      : CONTENT_SETTING_BLOCK));
  }

  std::unique_ptr<base::Value> BrowserToExtensionPref(
      const base::Value* browser_pref,
      bool is_incognito_profile) override {
    auto protected_identifier_mode =
        static_cast<ContentSetting>(browser_pref->GetInt());
    return std::make_unique<base::Value>(protected_identifier_mode ==
                                         CONTENT_SETTING_ALLOW);
  }
};

class PrefMapping {
 public:
  PrefMapping(const PrefMapping&) = delete;
  PrefMapping& operator=(const PrefMapping&) = delete;

  static PrefMapping* GetInstance() {
    return base::Singleton<PrefMapping>::get();
  }

  bool FindBrowserPrefForExtensionPref(const std::string& extension_pref,
                                       std::string* browser_pref,
                                       APIPermissionID* read_permission,
                                       APIPermissionID* write_permission) {
    auto it = mapping_.find(extension_pref);
    if (it != mapping_.end()) {
      *browser_pref = it->second.pref_name;
      *read_permission = it->second.read_permission;
      *write_permission = it->second.write_permission;
      return true;
    }
    return false;
  }

  bool FindEventForBrowserPref(const std::string& browser_pref,
                               std::string* event_name,
                               APIPermissionID* permission) {
    auto it = event_mapping_.find(browser_pref);
    if (it != event_mapping_.end()) {
      *event_name = it->second.pref_name;
      *permission = it->second.read_permission;
      return true;
    }
    return false;
  }

  PrefTransformerInterface* FindTransformerForBrowserPref(
      const std::string& browser_pref) {
    auto it = transformers_.find(browser_pref);
    if (it != transformers_.end())
      return it->second.get();
    return identity_transformer_.get();
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Given a pref name for an extension-controlled pref where the underlying
  // pref is controlled by ash, returns the PrefPath used by the crosapi to set
  // the pref in ash, or nullptr if no pref exists.
  crosapi::mojom::PrefPath GetPrefPathForPrefName(
      const std::string& pref_name) {
    static base::NoDestructor<std::map<std::string, crosapi::mojom::PrefPath>>
        name_to_extension_prefpath(
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
              crosapi::mojom::PrefPath::
                  kExtensionAccessibilitySpokenFeedbackEnabled},
             {chromeos::prefs::kAccessibilityStickyKeysEnabled,
              crosapi::mojom::PrefPath::kAccessibilityStickyKeysEnabled},
             {chromeos::prefs::kAccessibilitySwitchAccessEnabled,
              crosapi::mojom::PrefPath::kAccessibilitySwitchAccessEnabled},
             {chromeos::prefs::kAccessibilityVirtualKeyboardEnabled,
              crosapi::mojom::PrefPath::kAccessibilityVirtualKeyboardEnabled},
             {prefs::kProtectedContentDefault,
              crosapi::mojom::PrefPath::kProtectedContentDefault}});
    auto pref_path = name_to_extension_prefpath->find(pref_name);
    return pref_path == name_to_extension_prefpath->end()
               ? crosapi::mojom::PrefPath::kUnknown
               : pref_path->second;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

 private:
  friend struct base::DefaultSingletonTraits<PrefMapping>;
  PrefMapping() {
    identity_transformer_ = std::make_unique<IdentityPrefTransformer>();
    for (const auto& pref : kPrefMapping) {
      mapping_[pref.extension_pref] = PrefMapData(
          pref.browser_pref, pref.read_permission, pref.write_permission);
      std::string event_name =
          base::StringPrintf(kOnPrefChangeFormat, pref.extension_pref);
      event_mapping_[pref.browser_pref] =
          PrefMapData(event_name, pref.read_permission, pref.write_permission);
    }
    DCHECK_EQ(std::size(kPrefMapping), mapping_.size());
    DCHECK_EQ(std::size(kPrefMapping), event_mapping_.size());
    RegisterPrefTransformer(proxy_config::prefs::kProxy,
                            std::make_unique<ProxyPrefTransformer>());
    RegisterPrefTransformer(prefs::kCookieControlsMode,
                            std::make_unique<CookieControlsModeTransformer>());
    RegisterPrefTransformer(prefetch::prefs::kNetworkPredictionOptions,
                            std::make_unique<NetworkPredictionTransformer>());
    RegisterPrefTransformer(
        prefs::kProtectedContentDefault,
        std::make_unique<ProtectedContentEnabledTransformer>());
  }

  ~PrefMapping() = default;

  void RegisterPrefTransformer(
      const std::string& browser_pref,
      std::unique_ptr<PrefTransformerInterface> transformer) {
    DCHECK(!base::Contains(transformers_, browser_pref))
        << "Trying to register pref transformer for " << browser_pref
        << " twice";
    transformers_[browser_pref] = std::move(transformer);
  }

  struct PrefMapData {
    PrefMapData() = default;

    PrefMapData(const std::string& pref_name,
                APIPermissionID read,
                APIPermissionID write)
        : pref_name(pref_name),
          read_permission(read),
          write_permission(write) {}

    // Browser or extension preference to which the data maps.
    std::string pref_name;

    // Permission needed to read the preference.
    APIPermissionID read_permission = APIPermissionID::kInvalid;

    // Permission needed to write the preference.
    APIPermissionID write_permission = APIPermissionID::kInvalid;
  };

  using PrefMap = std::map<std::string, PrefMapData>;

  // Mapping from extension pref keys to browser pref keys and permissions.
  PrefMap mapping_;

  // Mapping from browser pref keys to extension event names and permissions.
  PrefMap event_mapping_;

  // Mapping from browser pref keys to transformers.
  std::map<std::string, std::unique_ptr<PrefTransformerInterface>>
      transformers_;

  std::unique_ptr<PrefTransformerInterface> identity_transformer_;
};

constexpr char kIncognitoPersistent[] = "incognito_persistent";
constexpr char kIncognitoSessionOnly[] = "incognito_session_only";
constexpr char kRegular[] = "regular";
constexpr char kRegularOnly[] = "regular_only";

// TODO(crbug.com/1366445): Consider using the ChromeSettingScope
// enum instead of ExtensionPrefsScope. That way, we could remove
// this function and the preceding string constants.
bool StringToScope(const std::string& s, ExtensionPrefsScope* scope) {
  if (s == kRegular) {
    *scope = kExtensionPrefsScopeRegular;
  } else if (s == kRegularOnly) {
    *scope = kExtensionPrefsScopeRegularOnly;
  } else if (s == kIncognitoPersistent) {
    *scope = kExtensionPrefsScopeIncognitoPersistent;
  } else if (s == kIncognitoSessionOnly) {
    *scope = kExtensionPrefsScopeIncognitoSessionOnly;
  } else {
    return false;
  }
  return true;
}

}  // namespace

PreferenceEventRouter::PreferenceEventRouter(Profile* profile)
    : profile_(profile) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Versions of ash without this capability cannot create observers for prefs
  // writing to the ash standalone browser prefstore.
  constexpr char kExtensionControlledPrefObserversCapability[] =
      "crbug/1334985";
  bool ash_supports_crosapi_observers =
      chromeos::BrowserParamsProxy::Get()->AshCapabilities().has_value() &&
      base::Contains(
          chromeos::BrowserParamsProxy::Get()->AshCapabilities().value(),
          kExtensionControlledPrefObserversCapability);
#endif

  registrar_.Init(profile_->GetPrefs());
  for (const auto& pref : kPrefMapping) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    crosapi::mojom::PrefPath pref_path =
        PrefMapping::GetInstance()->GetPrefPathForPrefName(pref.browser_pref);
    if (pref_path != crosapi::mojom::PrefPath::kUnknown &&
        ash_supports_crosapi_observers) {
      // Extension-controlled pref with the real value to watch in ash.
      // This base::Unretained() is safe because PreferenceEventRouter owns
      // the corresponding observer.
      extension_pref_observers_.push_back(std::make_unique<CrosapiPrefObserver>(
          pref_path,
          base::BindRepeating(&PreferenceEventRouter::OnAshPrefChanged,
                              base::Unretained(this), pref_path,
                              pref.extension_pref, pref.browser_pref)));
      registrar_.Add(
          pref.browser_pref,
          base::BindRepeating(&PreferenceEventRouter::OnControlledPrefChanged,
                              base::Unretained(this), registrar_.prefs()));
      continue;
    }
#endif
    registrar_.Add(
        pref.browser_pref,
        base::BindRepeating(&PreferenceEventRouter::OnPrefChanged,
                            base::Unretained(this), registrar_.prefs()));
  }
  DCHECK(!profile_->IsOffTheRecord());
  observed_profiles_.AddObservation(profile_.get());
  if (profile->HasPrimaryOTRProfile())
    OnOffTheRecordProfileCreated(
        profile->GetPrimaryOTRProfile(/*create_if_needed=*/true));
  else
    ObserveOffTheRecordPrefs(profile->GetReadOnlyOffTheRecordPrefs());
}

PreferenceEventRouter::~PreferenceEventRouter() = default;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void PreferenceEventRouter::OnControlledPrefChanged(
    PrefService* pref_service,
    const std::string& browser_pref) {
  // This pref has a corresponding value in ash. We should send the updated
  // value of the pref to ash.
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service ||
      !lacros_service->IsAvailable<crosapi::mojom::Prefs>()) {
    // Without the service, we cannot update this pref in ash.
    LOG(ERROR) << ErrorUtils::FormatErrorMessage(
        "API unavailable to set pref * in ash.", browser_pref);
    return;
  }

  crosapi::mojom::PrefPath pref_path =
      PrefMapping::GetInstance()->GetPrefPathForPrefName(browser_pref);
  // Should be a known pref path. Otherwise we would not have created this
  // observer.
  DCHECK(pref_path != crosapi::mojom::PrefPath::kUnknown);

  const PrefService::Preference* pref =
      pref_service->FindPreference(browser_pref);
  CHECK(pref);
  if (pref->IsExtensionControlled()) {
    // The pref has been set in lacros by an extension.
    // Transmit the value to ash to be stored in the standalone browser
    // prefstore.
    lacros_service->GetRemote<crosapi::mojom::Prefs>()->SetPref(
        pref_path, pref->GetValue()->Clone(), base::OnceClosure());
  } else {
    // The pref hasn't been set in lacros.
    // Remove any value from the standalone browser prefstore in ash.
    lacros_service->GetRemote<crosapi::mojom::Prefs>()
        ->ClearExtensionControlledPref(pref_path, base::OnceClosure());
  }
}

void PreferenceEventRouter::OnAshPrefChanged(crosapi::mojom::PrefPath pref_path,
                                             const std::string& extension_pref,
                                             const std::string& browser_pref,
                                             base::Value value) {
  // This pref should be read from ash.
  // We can only get here via callback from ash. So there should be a
  // LacrosService.
  auto* lacros_service = chromeos::LacrosService::Get();
  DCHECK(lacros_service);

  // It's not sufficient to have the new state of the pref - we also need
  // information about what just set it. So call Ash again to get information
  // about the control state.
  lacros_service->GetRemote<crosapi::mojom::Prefs>()
      ->GetExtensionPrefWithControl(
          pref_path, base::BindOnce(&PreferenceEventRouter::OnAshGetSuccess,
                                    weak_factory_.GetWeakPtr(), browser_pref));
}

void PreferenceEventRouter::OnAshGetSuccess(
    const std::string& browser_pref,
    absl::optional<::base::Value> opt_value,
    crosapi::mojom::PrefControlState control_state) {
  bool incognito = false;

  std::string event_name;
  APIPermissionID permission = APIPermissionID::kInvalid;
  bool found_event = PrefMapping::GetInstance()->FindEventForBrowserPref(
      browser_pref, &event_name, &permission);
  DCHECK(found_event);

  base::ListValue args;
  PrefTransformerInterface* transformer =
      PrefMapping::GetInstance()->FindTransformerForBrowserPref(browser_pref);

  base::Value* pref_value = &opt_value.value();
  std::unique_ptr<base::Value> transformed_value =
      transformer->BrowserToExtensionPref(pref_value, incognito);
  if (!transformed_value) {
    LOG(ERROR) << ErrorUtils::FormatErrorMessage(kConversionErrorMessage,
                                                 browser_pref);
    return;
  }

  base::Value::Dict dict;
  dict.Set(kValue, std::move(*transformed_value));
  args.Append(std::move(dict));

  events::HistogramValue histogram_value =
      events::TYPES_CHROME_SETTING_ON_CHANGE;
  extensions::preference_helpers::DispatchEventToExtensionsWithAshControlState(
      profile_, histogram_value, event_name, &args, permission, incognito,
      browser_pref, control_state);
}
#endif

void PreferenceEventRouter::OnPrefChanged(PrefService* pref_service,
                                          const std::string& browser_pref) {
  bool incognito = (pref_service != profile_->GetPrefs());

  std::string event_name;
  APIPermissionID permission = APIPermissionID::kInvalid;
  bool rv = PrefMapping::GetInstance()->FindEventForBrowserPref(
      browser_pref, &event_name, &permission);
  DCHECK(rv);

  base::ListValue args;
  const PrefService::Preference* pref =
      pref_service->FindPreference(browser_pref);
  CHECK(pref);
  PrefTransformerInterface* transformer =
      PrefMapping::GetInstance()->FindTransformerForBrowserPref(browser_pref);
  std::unique_ptr<base::Value> transformed_value =
      transformer->BrowserToExtensionPref(pref->GetValue(), incognito);
  if (!transformed_value) {
    LOG(ERROR) << ErrorUtils::FormatErrorMessage(kConversionErrorMessage,
                                                 pref->name());
    return;
  }

  base::Value::Dict dict;
  dict.Set(kValue, std::move(*transformed_value));
  if (incognito) {
    ExtensionPrefs* ep = ExtensionPrefs::Get(profile_);
    dict.Set(kIncognitoSpecific, ep->HasIncognitoPrefValue(browser_pref));
  }
  args.Append(std::move(dict));

  // TODO(kalman): Have a histogram value for each pref type.
  // This isn't so important for the current use case of these
  // histograms, which is to track which event types are waking up event
  // pages, or which are delivered to persistent background pages. Simply
  // "a setting changed" is enough detail for that. However if we try to
  // use these histograms for any fine-grained logic (like removing the
  // string event name altogether), or if we discover this event is
  // firing a lot and want to understand that better, then this will need
  // to change.
  events::HistogramValue histogram_value =
      events::TYPES_CHROME_SETTING_ON_CHANGE;
  extensions::preference_helpers::DispatchEventToExtensions(
      profile_, histogram_value, event_name, &args, permission, incognito,
      browser_pref);
}

void PreferenceEventRouter::OnOffTheRecordProfileCreated(
    Profile* off_the_record) {
  observed_profiles_.AddObservation(off_the_record);
  ObserveOffTheRecordPrefs(off_the_record->GetPrefs());
}

void PreferenceEventRouter::OnProfileWillBeDestroyed(Profile* profile) {
  observed_profiles_.RemoveObservation(profile);
  if (profile->IsOffTheRecord()) {
    // The real PrefService is about to be destroyed so we must make sure we
    // get the "dummy" one.
    ObserveOffTheRecordPrefs(profile_->GetReadOnlyOffTheRecordPrefs());
  }
}

void PreferenceEventRouter::ObserveOffTheRecordPrefs(PrefService* prefs) {
  incognito_registrar_ = std::make_unique<PrefChangeRegistrar>();
  incognito_registrar_->Init(prefs);
  for (const auto& pref : kPrefMapping) {
    incognito_registrar_->Add(
        pref.browser_pref,
        base::BindRepeating(&PreferenceEventRouter::OnPrefChanged,
                            base::Unretained(this),
                            incognito_registrar_->prefs()));
  }
}

PreferenceAPI::PreferenceAPI(content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)),
      prefs_helper_(
          ExtensionPrefs::Get(profile_),
          ExtensionPrefValueMapFactory::GetForBrowserContext(profile_)) {
  for (const auto& pref : kPrefMapping) {
    std::string event_name;
    APIPermissionID permission = APIPermissionID::kInvalid;
    bool rv = PrefMapping::GetInstance()->FindEventForBrowserPref(
        pref.browser_pref, &event_name, &permission);
    DCHECK(rv);
    EventRouter::Get(profile_)->RegisterObserver(this, event_name);
  }
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // On lacros, ensure the PreferenceEventRouter is always created to watch for
  // and notify of any pref changes, even if there's no extension listeners.
  // TODO(crbug.com/1334829): Abstract out lacros logic from the
  // PreferenceEventRouter so we don't needlessly dispatch extension events.
  EnsurePreferenceEventRouterCreated();
#endif
  content_settings_store()->AddObserver(this);
}

PreferenceAPI::~PreferenceAPI() = default;

void PreferenceAPI::Shutdown() {
  EventRouter::Get(profile_)->UnregisterObserver(this);
  if (!prefs_helper_.prefs()->extensions_disabled())
    ClearIncognitoSessionOnlyContentSettings();
  content_settings_store()->RemoveObserver(this);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<PreferenceAPI>>::
    DestructorAtExit g_preference_api_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<PreferenceAPI>*
PreferenceAPI::GetFactoryInstance() {
  return g_preference_api_factory.Pointer();
}

// static
PreferenceAPI* PreferenceAPI::Get(content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<PreferenceAPI>::Get(context);
}

void PreferenceAPI::OnListenerAdded(const EventListenerInfo& details) {
  EnsurePreferenceEventRouterCreated();
  EventRouter::Get(profile_)->UnregisterObserver(this);
}

void PreferenceAPI::EnsurePreferenceEventRouterCreated() {
  if (!preference_event_router_) {
    preference_event_router_ =
        std::make_unique<PreferenceEventRouter>(profile_);
  }
}

void PreferenceAPI::OnContentSettingChanged(const std::string& extension_id,
                                            bool incognito) {
  if (incognito) {
    prefs_helper_.prefs()->UpdateExtensionPref(
        extension_id, pref_names::kPrefIncognitoContentSettings,
        base::Value::ToUniquePtrValue(
            base::Value(content_settings_store()->GetSettingsForExtension(
                extension_id, kExtensionPrefsScopeIncognitoPersistent))));
  } else {
    prefs_helper_.prefs()->UpdateExtensionPref(
        extension_id, pref_names::kPrefContentSettings,
        base::Value::ToUniquePtrValue(
            base::Value(content_settings_store()->GetSettingsForExtension(
                extension_id, kExtensionPrefsScopeRegular))));
  }
}

void PreferenceAPI::ClearIncognitoSessionOnlyContentSettings() {
  ExtensionIdList extension_ids;
  prefs_helper_.prefs()->GetExtensions(&extension_ids);
  for (const auto& id : extension_ids) {
    content_settings_store()->ClearContentSettingsForExtension(
        id, kExtensionPrefsScopeIncognitoSessionOnly);
  }
}

scoped_refptr<ContentSettingsStore> PreferenceAPI::content_settings_store() {
  return ContentSettingsService::Get(profile_)->content_settings_store();
}

template <>
void
BrowserContextKeyedAPIFactory<PreferenceAPI>::DeclareFactoryDependencies() {
  DependsOn(ContentSettingsService::GetFactoryInstance());
  DependsOn(ExtensionPrefsFactory::GetInstance());
  DependsOn(ExtensionPrefValueMapFactory::GetInstance());
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}

PreferenceFunction::~PreferenceFunction() = default;

GetPreferenceFunction::~GetPreferenceFunction() = default;

ExtensionFunction::ResponseAction GetPreferenceFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 2);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_string());
  EXTENSION_FUNCTION_VALIDATE(args()[1].is_dict());

  const std::string& pref_key = args()[0].GetString();
  const base::Value& details = args()[1];

  bool incognito = false;
  if (absl::optional<bool> result = details.FindBoolKey(kIncognitoKey)) {
    incognito = *result;
  }

  // Check incognito access.
  if (incognito) {
    // Extensions are only allowed to modify incognito preferences if they are
    // enabled in incognito. If the calling browser context is off the record,
    // then the extension must be allowed to run incognito. Otherwise, this
    // could be a spanning mode extension, and we need to check its incognito
    // access.
    if (!browser_context()->IsOffTheRecord() &&
        !include_incognito_information()) {
      return RespondNow(Error(extension_misc::kIncognitoErrorMessage));
    }
  }

  // Obtain and check read/write permission for pref.
  std::string browser_pref;
  APIPermissionID read_permission = APIPermissionID::kInvalid;
  APIPermissionID write_permission = APIPermissionID::kInvalid;
  EXTENSION_FUNCTION_VALIDATE(
      PrefMapping::GetInstance()->FindBrowserPrefForExtensionPref(
      pref_key, &browser_pref, &read_permission, &write_permission));
  if (!extension()->permissions_data()->HasAPIPermission(read_permission))
    return RespondNow(Error(kPermissionErrorMessage, pref_key));

  Profile* profile = Profile::FromBrowserContext(browser_context());

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Check whether this is a lacros extension controlled pref.
  cached_browser_pref_ = browser_pref;
  crosapi::mojom::PrefPath pref_path =
      PrefMapping::GetInstance()->GetPrefPathForPrefName(cached_browser_pref_);
  if (pref_path != crosapi::mojom::PrefPath::kUnknown) {
    if (!profile->IsMainProfile()) {
      return RespondNow(Error(kPrimaryProfileOnlyErrorMessage, pref_key));
    }
    // This pref should be read from ash.
    auto* lacros_service = chromeos::LacrosService::Get();
    if (!lacros_service ||
        !lacros_service->IsAvailable<crosapi::mojom::Prefs>()) {
      return RespondNow(Error("OS Service is unavailable."));
    }
    lacros_service->GetRemote<crosapi::mojom::Prefs>()
        ->GetExtensionPrefWithControl(
            pref_path,
            base::BindOnce(&GetPreferenceFunction::OnLacrosGetSuccess, this));
    return RespondLater();
  }
#endif

  PrefService* prefs =
      extensions::preference_helpers::GetProfilePrefService(profile, incognito);

  const PrefService::Preference* pref = prefs->FindPreference(browser_pref);
  CHECK(pref);

  // Retrieve level of control.
  std::string level_of_control =
      extensions::preference_helpers::GetLevelOfControl(
          profile, extension_id(), browser_pref, incognito);

  base::Value result(base::Value::Type::DICTIONARY);
  ProduceGetResult(&result, pref->GetValue(), level_of_control, browser_pref,
                   incognito);

  return RespondNow(OneArgument(std::move(result)));
}

void GetPreferenceFunction::ProduceGetResult(
    base::Value* result,
    const base::Value* pref_value,
    const std::string& level_of_control,
    const std::string& browser_pref,
    bool incognito) {
  PrefTransformerInterface* transformer =
      PrefMapping::GetInstance()->FindTransformerForBrowserPref(browser_pref);
  std::unique_ptr<base::Value> transformed_value =
      transformer->BrowserToExtensionPref(pref_value, incognito);
  if (!transformed_value) {
    // TODO(devlin): Can this happen?  When?  Should it be an error, or a bad
    // message?
    LOG(ERROR) << ErrorUtils::FormatErrorMessage(kConversionErrorMessage,
                                                 browser_pref);
    return;
  }

  result->SetKey(kValue,
                 base::Value::FromUniquePtrValue(std::move(transformed_value)));
  result->SetStringKey(kLevelOfControl, level_of_control);

  // Retrieve incognito status.
  if (incognito) {
    ExtensionPrefs* ep = ExtensionPrefs::Get(browser_context());
    result->SetBoolKey(kIncognitoSpecific,
                       ep->HasIncognitoPrefValue(browser_pref));
  }
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void GetPreferenceFunction::OnLacrosGetSuccess(
    absl::optional<::base::Value> opt_value,
    crosapi::mojom::PrefControlState control_state) {
  if (!browser_context()) {
    return;
  }

  // Get read/write permissions and pref name again.
  Profile* profile = Profile::FromBrowserContext(browser_context());

  std::string pref_key = args()[0].GetString();
  const base::Value& details = args()[1];

  bool incognito = false;
  if (absl::optional<bool> result = details.FindBoolKey(kIncognitoKey)) {
    incognito = *result;
  }

  ::base::Value* pref_value = &opt_value.value();

  std::string level_of_control;
  level_of_control =
      extensions::preference_helpers::GetLevelOfControlWithAshControlState(
          control_state, profile, extension_id(), cached_browser_pref_,
          incognito);

  base::Value result(base::Value::Type::DICTIONARY);

  ProduceGetResult(&result, pref_value, level_of_control, cached_browser_pref_,
                   incognito);

  Respond(OneArgument(std::move(result)));
}
#endif

SetPreferenceFunction::~SetPreferenceFunction() = default;

ExtensionFunction::ResponseAction SetPreferenceFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 2);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_string());
  EXTENSION_FUNCTION_VALIDATE(args()[1].is_dict());

  std::string pref_key = args()[0].GetString();
  const base::Value::Dict& details = args()[1].GetDict();

  const base::Value* value = details.Find(kValue);
  EXTENSION_FUNCTION_VALIDATE(value);

  ExtensionPrefsScope scope = kExtensionPrefsScopeRegular;
  if (const std::string* scope_str = details.FindString(kScopeKey)) {
    EXTENSION_FUNCTION_VALIDATE(StringToScope(*scope_str, &scope));
  }

  // Check incognito scope.
  bool incognito =
      (scope == kExtensionPrefsScopeIncognitoPersistent ||
       scope == kExtensionPrefsScopeIncognitoSessionOnly);
  if (incognito) {
    // Regular profiles can't access incognito unless
    // include_incognito_information is true.
    if (!browser_context()->IsOffTheRecord() &&
        !include_incognito_information())
      return RespondNow(Error(extension_misc::kIncognitoErrorMessage));
  } else if (browser_context()->IsOffTheRecord()) {
    // If the browser_context associated with this ExtensionFunction is off the
    // record, it must have come from the incognito process for a split-mode
    // extension (spanning mode extensions only run in the on-the-record
    // process). The incognito profile of a split-mode extension should never be
    // able to modify the on-the-record profile, so error out.
    return RespondNow(
        Error("Can't modify regular settings from an incognito context."));
  }

  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (scope == kExtensionPrefsScopeIncognitoSessionOnly &&
      !profile->HasPrimaryOTRProfile()) {
    return RespondNow(Error(extension_misc::kIncognitoSessionOnlyErrorMessage));
  }

  // Obtain pref.
  std::string browser_pref;
  APIPermissionID read_permission = APIPermissionID::kInvalid;
  APIPermissionID write_permission = APIPermissionID::kInvalid;
  EXTENSION_FUNCTION_VALIDATE(
      PrefMapping::GetInstance()->FindBrowserPrefForExtensionPref(
      pref_key, &browser_pref, &read_permission, &write_permission));
  if (!extension()->permissions_data()->HasAPIPermission(write_permission))
    return RespondNow(Error(kPermissionErrorMessage, pref_key));

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If the pref is ash-controlled, check that the service is present.
  // If it isn't, don't allow the pref to be set.
  crosapi::mojom::PrefPath pref_path =
      PrefMapping::GetInstance()->GetPrefPathForPrefName(browser_pref);
  chromeos::LacrosService* lacros_service;
  if (pref_path != crosapi::mojom::PrefPath::kUnknown) {
    if (!profile->IsMainProfile()) {
      return RespondNow(Error(kPrimaryProfileOnlyErrorMessage, pref_key));
    }
    // This pref should be set in ash.
    // Check that the service exists so we can set it.
    lacros_service = chromeos::LacrosService::Get();
    if (!lacros_service ||
        !lacros_service->IsAvailable<crosapi::mojom::Prefs>()) {
      return RespondNow(Error("OS Service is unavailable."));
    }
  }
#endif

  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context());
  const PrefService::Preference* pref =
      prefs->pref_service()->FindPreference(browser_pref);
  CHECK(pref);

  // Validate new value.
  PrefTransformerInterface* transformer =
      PrefMapping::GetInstance()->FindTransformerForBrowserPref(browser_pref);
  std::string error;
  bool bad_message = false;
  std::unique_ptr<base::Value> browser_pref_value(
      transformer->ExtensionToBrowserPref(value, &error, &bad_message));
  if (!browser_pref_value) {
    EXTENSION_FUNCTION_VALIDATE(!bad_message);
    return RespondNow(Error(std::move(error)));
  }
  EXTENSION_FUNCTION_VALIDATE(browser_pref_value->type() == pref->GetType());

  // Validate also that the stored value can be converted back by the
  // transformer.
  std::unique_ptr<base::Value> extension_pref_value(
      transformer->BrowserToExtensionPref(browser_pref_value.get(), incognito));
  EXTENSION_FUNCTION_VALIDATE(extension_pref_value);

  PreferenceAPI* preference_api = PreferenceAPI::Get(browser_context());

  // Set the new Autofill prefs if the extension sets the deprecated pref in
  // order to maintain backward compatibility in the extensions preference API.
  // TODO(crbug.com/870328): Remove this once the deprecated pref is retired.
  if (autofill::prefs::kAutofillEnabledDeprecated == browser_pref) {
    // |SetExtensionControlledPref| takes ownership of the base::Value pointer.
    preference_api->SetExtensionControlledPref(
        extension_id(), autofill::prefs::kAutofillCreditCardEnabled, scope,
        base::Value(browser_pref_value->GetBool()));
    preference_api->SetExtensionControlledPref(
        extension_id(), autofill::prefs::kAutofillProfileEnabled, scope,
        base::Value(browser_pref_value->GetBool()));
  }

  // Whenever an extension takes control of the |kSafeBrowsingEnabled|
  // preference, it must also set |kSafeBrowsingEnhanced| to false.
  // See crbug.com/1064722 for more background.
  //
  // TODO(crbug.com/1064722): Consider extending
  // chrome.privacy.services.safeBrowsingEnabled to a three-state enum.
  if (prefs::kSafeBrowsingEnabled == browser_pref) {
    preference_api->SetExtensionControlledPref(extension_id(),
                                               prefs::kSafeBrowsingEnhanced,
                                               scope, base::Value(false));
  }

  base::Value val =
      base::Value::FromUniquePtrValue(std::move(browser_pref_value));

  preference_api->SetExtensionControlledPref(extension_id(), browser_pref,
                                             scope, val.Clone());
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (pref_path != crosapi::mojom::PrefPath::kUnknown &&
      preference_api->DoesExtensionControlPref(extension_id(), browser_pref,
                                               nullptr)) {
    lacros_service->GetRemote<crosapi::mojom::Prefs>()->SetPref(
        pref_path, val.Clone(),
        base::BindOnce(&SetPreferenceFunction::OnLacrosSetSuccess, this));
    return RespondLater();
  }
#endif

  return RespondNow(NoArguments());
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void SetPreferenceFunction::OnLacrosSetSuccess() {
  Respond(NoArguments());
}
#endif

ClearPreferenceFunction::~ClearPreferenceFunction() = default;

ExtensionFunction::ResponseAction ClearPreferenceFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 2);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_string());
  EXTENSION_FUNCTION_VALIDATE(args()[1].is_dict());

  std::string pref_key = args()[0].GetString();
  const base::Value::Dict& details = args()[1].GetDict();

  ExtensionPrefsScope scope = kExtensionPrefsScopeRegular;
  if (const std::string* scope_str = details.FindString(kScopeKey)) {
    EXTENSION_FUNCTION_VALIDATE(StringToScope(*scope_str, &scope));
  }

  // Check incognito scope.
  bool incognito =
      (scope == kExtensionPrefsScopeIncognitoPersistent ||
       scope == kExtensionPrefsScopeIncognitoSessionOnly);
  if (incognito) {
    // We don't check incognito permissions here, as an extension should be
    // always allowed to clear its own settings.
  } else if (browser_context()->IsOffTheRecord()) {
    // Incognito profiles can't access regular mode ever, they only exist in
    // split mode.
    return RespondNow(
        Error("Can't modify regular settings from an incognito context."));
  }

  std::string browser_pref;
  APIPermissionID read_permission = APIPermissionID::kInvalid;
  APIPermissionID write_permission = APIPermissionID::kInvalid;
  EXTENSION_FUNCTION_VALIDATE(
      PrefMapping::GetInstance()->FindBrowserPrefForExtensionPref(
      pref_key, &browser_pref, &read_permission, &write_permission));
  if (!extension()->permissions_data()->HasAPIPermission(write_permission))
    return RespondNow(Error(kPermissionErrorMessage, pref_key));

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If the pref is ash-controlled, check that the service is present.
  // If it isn't, don't allow the pref to be cleared.
  crosapi::mojom::PrefPath pref_path =
      PrefMapping::GetInstance()->GetPrefPathForPrefName(browser_pref);
  chromeos::LacrosService* lacros_service;
  if (pref_path != crosapi::mojom::PrefPath::kUnknown) {
    Profile* profile = Profile::FromBrowserContext(browser_context());
    if (!profile->IsMainProfile()) {
      return RespondNow(Error(kPrimaryProfileOnlyErrorMessage, pref_key));
    }
    // This pref should be cleared in ash.
    lacros_service = chromeos::LacrosService::Get();
    if (!lacros_service ||
        !lacros_service->IsAvailable<crosapi::mojom::Prefs>()) {
      return RespondNow(Error("OS Service is unavailable."));
    }
  }
  bool did_just_control_pref =
      PreferenceAPI::Get(browser_context())
          ->DoesExtensionControlPref(extension_id(), browser_pref, nullptr);
#endif

  PreferenceAPI::Get(browser_context())
      ->RemoveExtensionControlledPref(extension_id(), browser_pref, scope);

  // Whenever an extension clears the |kSafeBrowsingEnabled| preference,
  // it must also clear |kSafeBrowsingEnhanced|. See crbug.com/1064722 for
  // more background.
  //
  // TODO(crbug.com/1064722): Consider extending
  // chrome.privacy.services.safeBrowsingEnabled to a three-state enum.
  if (prefs::kSafeBrowsingEnabled == browser_pref) {
    PreferenceAPI::Get(browser_context())
        ->RemoveExtensionControlledPref(extension_id(),
                                        prefs::kSafeBrowsingEnhanced, scope);
  }
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (pref_path != crosapi::mojom::PrefPath::kUnknown &&
      did_just_control_pref) {
    // This is an ash pref and we need to update ash because the extension that
    // just cleared the pref used to control it. Now, either another extension
    // of lower precedence controls the pref (in which case we update the pref
    // to that value), or no other extension has set the pref (in which case
    // we can clear the value set by extensions in ash).
    Profile* profile = Profile::FromBrowserContext(browser_context());
    PrefService* pref_service =
        extensions::preference_helpers::GetProfilePrefService(profile,
                                                              incognito);

    const PrefService::Preference* pref =
        pref_service->FindPreference(browser_pref);
    CHECK(pref);
    if (pref->IsExtensionControlled()) {
      lacros_service->GetRemote<crosapi::mojom::Prefs>()->SetPref(
          pref_path, pref->GetValue()->Clone(),
          base::BindOnce(&ClearPreferenceFunction::OnLacrosClearSuccess, this));
      return RespondLater();
    }
    // No extension in lacros is claiming this pref.
    lacros_service->GetRemote<crosapi::mojom::Prefs>()
        ->ClearExtensionControlledPref(
            pref_path,
            base::BindOnce(&ClearPreferenceFunction::OnLacrosClearSuccess,
                           this));
    return RespondLater();
  }
#endif
  return RespondNow(NoArguments());
}
#if BUILDFLAG(IS_CHROMEOS_LACROS)
void ClearPreferenceFunction::OnLacrosClearSuccess() {
  Respond(NoArguments());
}
#endif
}  // namespace extensions
