// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/preference/preference_api.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/singleton.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/content_settings/content_settings_service.h"
#include "chrome/browser/extensions/api/preference/preference_api_constants.h"
#include "chrome/browser/extensions/api/preference/preference_helpers.h"
#include "chrome/browser/extensions/api/proxy/proxy_api.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "media/media_buildflags.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/ash_pref_names.h"  // nogncheck
#endif

namespace extensions {

namespace {

struct PrefMappingEntry {
  // Name of the preference referenced by the extension API JSON.
  const char* extension_pref;

  // Name of the preference in the PrefStores.
  const char* browser_pref;

  // Permission required to read and observe this preference.
  // Use APIPermission::kInvalid for |read_permission| to express that the read
  // permission should not be granted.
  APIPermission::ID read_permission;

  // Permission required to write this preference.
  // Use APIPermission::kInvalid for |write_permission| to express that the
  // write permission should not be granted.
  APIPermission::ID write_permission;
};

const char kOnPrefChangeFormat[] = "types.ChromeSetting.%s.onChange";
const char kConversionErrorMessage[] =
    "Internal error: Stored value for preference '*' cannot be converted "
    "properly.";

const PrefMappingEntry kPrefMapping[] = {
    {"spdy_proxy.enabled", data_reduction_proxy::prefs::kDataSaverEnabled,
     APIPermission::kDataReductionProxy, APIPermission::kDataReductionProxy},
    {"data_reduction.daily_original_length",
     data_reduction_proxy::prefs::kDailyHttpOriginalContentLength,
     APIPermission::kDataReductionProxy, APIPermission::kDataReductionProxy},
    {"data_reduction.daily_received_length",
     data_reduction_proxy::prefs::kDailyHttpReceivedContentLength,
     APIPermission::kDataReductionProxy, APIPermission::kDataReductionProxy},
    {"data_usage_reporting.enabled",
     data_reduction_proxy::prefs::kDataUsageReportingEnabled,
     APIPermission::kDataReductionProxy, APIPermission::kDataReductionProxy},
    {"alternateErrorPagesEnabled", prefs::kAlternateErrorPagesEnabled,
     APIPermission::kPrivacy, APIPermission::kPrivacy},
    {"autofillEnabled", autofill::prefs::kAutofillEnabledDeprecated,
     APIPermission::kPrivacy, APIPermission::kPrivacy},
    {"autofillAddressEnabled", autofill::prefs::kAutofillProfileEnabled,
     APIPermission::kPrivacy, APIPermission::kPrivacy},
    {"autofillCreditCardEnabled", autofill::prefs::kAutofillCreditCardEnabled,
     APIPermission::kPrivacy, APIPermission::kPrivacy},
    {"hyperlinkAuditingEnabled", prefs::kEnableHyperlinkAuditing,
     APIPermission::kPrivacy, APIPermission::kPrivacy},
    {"networkPredictionEnabled", prefs::kNetworkPredictionOptions,
     APIPermission::kPrivacy, APIPermission::kPrivacy},
    {"passwordSavingEnabled",
     password_manager::prefs::kCredentialsEnableService,
     APIPermission::kPrivacy, APIPermission::kPrivacy},
    {"protectedContentEnabled", prefs::kEnableDRM, APIPermission::kPrivacy,
     APIPermission::kPrivacy},
    {"proxy", proxy_config::prefs::kProxy, APIPermission::kProxy,
     APIPermission::kProxy},
    {"referrersEnabled", prefs::kEnableReferrers, APIPermission::kPrivacy,
     APIPermission::kPrivacy},
    {"doNotTrackEnabled", prefs::kEnableDoNotTrack, APIPermission::kPrivacy,
     APIPermission::kPrivacy},
    {"safeBrowsingEnabled", prefs::kSafeBrowsingEnabled,
     APIPermission::kPrivacy, APIPermission::kPrivacy},
    {"safeBrowsingExtendedReportingEnabled",
     prefs::kSafeBrowsingScoutReportingEnabled, APIPermission::kPrivacy,
     APIPermission::kPrivacy},
    {"searchSuggestEnabled", prefs::kSearchSuggestEnabled,
     APIPermission::kPrivacy, APIPermission::kPrivacy},
    {"spellingServiceEnabled", spellcheck::prefs::kSpellCheckUseSpellingService,
     APIPermission::kPrivacy, APIPermission::kPrivacy},
    {"thirdPartyCookiesAllowed", prefs::kBlockThirdPartyCookies,
     APIPermission::kPrivacy, APIPermission::kPrivacy},
    {"translationServiceEnabled", prefs::kOfferTranslateEnabled,
     APIPermission::kPrivacy, APIPermission::kPrivacy},
    // webRTCMultipleRoutesEnabled and webRTCNonProxiedUdpEnabled have been
    // replaced by webRTCIPHandlingPolicy. Leaving it for backward
    // compatibility. TODO(guoweis): Remove this in M50.
    {"webRTCMultipleRoutesEnabled", prefs::kWebRTCMultipleRoutesEnabled,
     APIPermission::kPrivacy, APIPermission::kPrivacy},
    {"webRTCNonProxiedUdpEnabled", prefs::kWebRTCNonProxiedUdpEnabled,
     APIPermission::kPrivacy, APIPermission::kPrivacy},
    {"webRTCIPHandlingPolicy", prefs::kWebRTCIPHandlingPolicy,
     APIPermission::kPrivacy, APIPermission::kPrivacy},
    {"webRTCUDPPortRange", prefs::kWebRTCUDPPortRange, APIPermission::kPrivacy,
     APIPermission::kPrivacy},
    // accessibilityFeatures.animationPolicy is available for
    // all platforms but the others from accessibilityFeatures
    // is only available for OS_CHROMEOS.
    {"animationPolicy", prefs::kAnimationPolicy,
     APIPermission::kAccessibilityFeaturesRead,
     APIPermission::kAccessibilityFeaturesModify},
#if defined(OS_CHROMEOS)
    {"autoclick", ash::prefs::kAccessibilityAutoclickEnabled,
     APIPermission::kAccessibilityFeaturesRead,
     APIPermission::kAccessibilityFeaturesModify},
    {"highContrast", ash::prefs::kAccessibilityHighContrastEnabled,
     APIPermission::kAccessibilityFeaturesRead,
     APIPermission::kAccessibilityFeaturesModify},
    {"largeCursor", ash::prefs::kAccessibilityLargeCursorEnabled,
     APIPermission::kAccessibilityFeaturesRead,
     APIPermission::kAccessibilityFeaturesModify},
    {"screenMagnifier", ash::prefs::kAccessibilityScreenMagnifierEnabled,
     APIPermission::kAccessibilityFeaturesRead,
     APIPermission::kAccessibilityFeaturesModify},
    {"selectToSpeak", ash::prefs::kAccessibilitySelectToSpeakEnabled,
     APIPermission::kAccessibilityFeaturesRead,
     APIPermission::kAccessibilityFeaturesModify},
    {"spokenFeedback", ash::prefs::kAccessibilitySpokenFeedbackEnabled,
     APIPermission::kAccessibilityFeaturesRead,
     APIPermission::kAccessibilityFeaturesModify},
    {"stickyKeys", ash::prefs::kAccessibilityStickyKeysEnabled,
     APIPermission::kAccessibilityFeaturesRead,
     APIPermission::kAccessibilityFeaturesModify},
    {"virtualKeyboard", ash::prefs::kAccessibilityVirtualKeyboardEnabled,
     APIPermission::kAccessibilityFeaturesRead,
     APIPermission::kAccessibilityFeaturesModify},
#endif
};

class IdentityPrefTransformer : public PrefTransformerInterface {
 public:
  std::unique_ptr<base::Value> ExtensionToBrowserPref(
      const base::Value* extension_pref,
      std::string* error,
      bool* bad_message) override {
    return extension_pref->CreateDeepCopy();
  }

  std::unique_ptr<base::Value> BrowserToExtensionPref(
      const base::Value* browser_pref) override {
    return browser_pref->CreateDeepCopy();
  }
};

class InvertBooleanTransformer : public PrefTransformerInterface {
 public:
  std::unique_ptr<base::Value> ExtensionToBrowserPref(
      const base::Value* extension_pref,
      std::string* error,
      bool* bad_message) override {
    return InvertBooleanValue(extension_pref);
  }

  std::unique_ptr<base::Value> BrowserToExtensionPref(
      const base::Value* browser_pref) override {
    return InvertBooleanValue(browser_pref);
  }

 private:
  static std::unique_ptr<base::Value> InvertBooleanValue(
      const base::Value* value) {
    bool bool_value = false;
    bool result = value->GetAsBoolean(&bool_value);
    DCHECK(result);
    return std::make_unique<base::Value>(!bool_value);
  }
};

class NetworkPredictionTransformer : public PrefTransformerInterface {
 public:
  std::unique_ptr<base::Value> ExtensionToBrowserPref(
      const base::Value* extension_pref,
      std::string* error,
      bool* bad_message) override {
    bool bool_value = false;
    const bool pref_found = extension_pref->GetAsBoolean(&bool_value);
    DCHECK(pref_found) << "Preference not found.";
    if (bool_value) {
      return std::make_unique<base::Value>(
          chrome_browser_net::NETWORK_PREDICTION_DEFAULT);
    }
    return std::make_unique<base::Value>(
        chrome_browser_net::NETWORK_PREDICTION_NEVER);
  }

  std::unique_ptr<base::Value> BrowserToExtensionPref(
      const base::Value* browser_pref) override {
    int int_value = chrome_browser_net::NETWORK_PREDICTION_DEFAULT;
    const bool pref_found = browser_pref->GetAsInteger(&int_value);
    DCHECK(pref_found) << "Preference not found.";
    return std::make_unique<base::Value>(
        int_value != chrome_browser_net::NETWORK_PREDICTION_NEVER);
  }
};

class PrefMapping {
 public:
  static PrefMapping* GetInstance() {
    return base::Singleton<PrefMapping>::get();
  }

  bool FindBrowserPrefForExtensionPref(const std::string& extension_pref,
                                       std::string* browser_pref,
                                       APIPermission::ID* read_permission,
                                       APIPermission::ID* write_permission) {
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
                               APIPermission::ID* permission) {
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
    DCHECK_EQ(base::size(kPrefMapping), mapping_.size());
    DCHECK_EQ(base::size(kPrefMapping), event_mapping_.size());
    RegisterPrefTransformer(proxy_config::prefs::kProxy,
                            std::make_unique<ProxyPrefTransformer>());
    RegisterPrefTransformer(prefs::kBlockThirdPartyCookies,
                            std::make_unique<InvertBooleanTransformer>());
    RegisterPrefTransformer(prefs::kNetworkPredictionOptions,
                            std::make_unique<NetworkPredictionTransformer>());
  }

  ~PrefMapping() {
  }

  void RegisterPrefTransformer(
      const std::string& browser_pref,
      std::unique_ptr<PrefTransformerInterface> transformer) {
    DCHECK(!base::Contains(transformers_, browser_pref))
        << "Trying to register pref transformer for " << browser_pref
        << " twice";
    transformers_[browser_pref] = std::move(transformer);
  }

  struct PrefMapData {
    PrefMapData()
        : read_permission(APIPermission::kInvalid),
          write_permission(APIPermission::kInvalid) {}

    PrefMapData(const std::string& pref_name,
                APIPermission::ID read,
                APIPermission::ID write)
        : pref_name(pref_name),
          read_permission(read),
          write_permission(write) {}

    // Browser or extension preference to which the data maps.
    std::string pref_name;

    // Permission needed to read the preference.
    APIPermission::ID read_permission;

    // Permission needed to write the preference.
    APIPermission::ID write_permission;
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

  DISALLOW_COPY_AND_ASSIGN(PrefMapping);
};

}  // namespace

PreferenceEventRouter::PreferenceEventRouter(Profile* profile)
    : profile_(profile) {
  registrar_.Init(profile_->GetPrefs());
  for (const auto& pref : kPrefMapping) {
    registrar_.Add(pref.browser_pref,
                   base::Bind(&PreferenceEventRouter::OnPrefChanged,
                              base::Unretained(this), registrar_.prefs()));
  }
  DCHECK(!profile_->IsOffTheRecord());
  observed_profiles_.Add(profile_);
  if (profile->HasOffTheRecordProfile())
    OnOffTheRecordProfileCreated(profile->GetOffTheRecordProfile());
  else
    ObserveOffTheRecordPrefs(profile->GetReadOnlyOffTheRecordPrefs());
}

PreferenceEventRouter::~PreferenceEventRouter() = default;

void PreferenceEventRouter::OnPrefChanged(PrefService* pref_service,
                                          const std::string& browser_pref) {
  bool incognito = (pref_service != profile_->GetPrefs());

  std::string event_name;
  APIPermission::ID permission = APIPermission::kInvalid;
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
      transformer->BrowserToExtensionPref(pref->GetValue());
  if (!transformed_value) {
    LOG(ERROR) << ErrorUtils::FormatErrorMessage(kConversionErrorMessage,
                                                 pref->name());
    return;
  }

  auto dict = std::make_unique<base::DictionaryValue>();
  dict->Set(extensions::preference_api_constants::kValue,
            std::move(transformed_value));
  if (incognito) {
    ExtensionPrefs* ep = ExtensionPrefs::Get(profile_);
    dict->SetBoolean(extensions::preference_api_constants::kIncognitoSpecific,
                     ep->HasIncognitoPrefValue(browser_pref));
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
  observed_profiles_.Add(off_the_record);
  ObserveOffTheRecordPrefs(off_the_record->GetPrefs());
}

void PreferenceEventRouter::OnProfileWillBeDestroyed(Profile* profile) {
  observed_profiles_.Remove(profile);
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
        base::Bind(&PreferenceEventRouter::OnPrefChanged,
                   base::Unretained(this), incognito_registrar_->prefs()));
  }
}

void PreferenceAPIBase::SetExtensionControlledPref(
    const std::string& extension_id,
    const std::string& pref_key,
    ExtensionPrefsScope scope,
    base::Value value) {
#ifndef NDEBUG
  const PrefService::Preference* pref =
      extension_prefs()->pref_service()->FindPreference(pref_key);
  DCHECK(pref) << "Extension controlled preference key " << pref_key
               << " not registered.";
  DCHECK_EQ(pref->GetType(), value.type())
      << "Extension controlled preference " << pref_key << " has wrong type.";
#endif

  std::string scope_string;
  // ScopeToPrefName() returns false if the scope is not persisted.
  if (pref_names::ScopeToPrefName(scope, &scope_string)) {
    // Also store in persisted Preferences file to recover after a
    // browser restart.
    ExtensionPrefs::ScopedDictionaryUpdate update(extension_prefs(),
                                                  extension_id,
                                                  scope_string);
    auto preference = update.Create();
    preference->SetWithoutPathExpansion(pref_key, value.CreateDeepCopy());
  }
  extension_pref_value_map()->SetExtensionPref(extension_id, pref_key, scope,
                                               std::move(value));
}

void PreferenceAPIBase::RemoveExtensionControlledPref(
    const std::string& extension_id,
    const std::string& pref_key,
    ExtensionPrefsScope scope) {
  DCHECK(extension_prefs()->pref_service()->FindPreference(pref_key))
      << "Extension controlled preference key " << pref_key
      << " not registered.";

  std::string scope_string;
  if (pref_names::ScopeToPrefName(scope, &scope_string)) {
    ExtensionPrefs::ScopedDictionaryUpdate update(extension_prefs(),
                                                  extension_id,
                                                  scope_string);
    auto preference = update.Get();
    if (preference)
      preference->RemoveWithoutPathExpansion(pref_key, nullptr);
  }
  extension_pref_value_map()->RemoveExtensionPref(
      extension_id, pref_key, scope);
}

bool PreferenceAPIBase::CanExtensionControlPref(
     const std::string& extension_id,
     const std::string& pref_key,
     bool incognito) {
  DCHECK(extension_prefs()->pref_service()->FindPreference(pref_key))
      << "Extension controlled preference key " << pref_key
      << " not registered.";

  return extension_pref_value_map()->CanExtensionControlPref(
       extension_id, pref_key, incognito);
}

bool PreferenceAPIBase::DoesExtensionControlPref(
    const std::string& extension_id,
    const std::string& pref_key,
    bool* from_incognito) {
  DCHECK(extension_prefs()->pref_service()->FindPreference(pref_key))
      << "Extension controlled preference key " << pref_key
      << " not registered.";

  return extension_pref_value_map()->DoesExtensionControlPref(
      extension_id, pref_key, from_incognito);
}

PreferenceAPI::PreferenceAPI(content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)) {
  for (const auto& pref : kPrefMapping) {
    std::string event_name;
    APIPermission::ID permission = APIPermission::kInvalid;
    bool rv = PrefMapping::GetInstance()->FindEventForBrowserPref(
        pref.browser_pref, &event_name, &permission);
    DCHECK(rv);
    EventRouter::Get(profile_)->RegisterObserver(this, event_name);
  }
  content_settings_store()->AddObserver(this);
}

PreferenceAPI::~PreferenceAPI() {
}

void PreferenceAPI::Shutdown() {
  EventRouter::Get(profile_)->UnregisterObserver(this);
  if (!extension_prefs()->extensions_disabled())
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
  preference_event_router_ = std::make_unique<PreferenceEventRouter>(profile_);
  EventRouter::Get(profile_)->UnregisterObserver(this);
}

void PreferenceAPI::OnContentSettingChanged(const std::string& extension_id,
                                            bool incognito) {
  if (incognito) {
    extension_prefs()->UpdateExtensionPref(
        extension_id,
        pref_names::kPrefIncognitoContentSettings,
        content_settings_store()->GetSettingsForExtension(
            extension_id, kExtensionPrefsScopeIncognitoPersistent));
  } else {
    extension_prefs()->UpdateExtensionPref(
        extension_id,
        pref_names::kPrefContentSettings,
        content_settings_store()->GetSettingsForExtension(
            extension_id, kExtensionPrefsScopeRegular));
  }
}

void PreferenceAPI::ClearIncognitoSessionOnlyContentSettings() {
  ExtensionIdList extension_ids;
  extension_prefs()->GetExtensions(&extension_ids);
  for (const auto& id : extension_ids) {
    content_settings_store()->ClearContentSettingsForExtension(
        id, kExtensionPrefsScopeIncognitoSessionOnly);
  }
}

ExtensionPrefs* PreferenceAPI::extension_prefs() {
  return ExtensionPrefs::Get(profile_);
}

ExtensionPrefValueMap* PreferenceAPI::extension_pref_value_map() {
  return ExtensionPrefValueMapFactory::GetForBrowserContext(profile_);
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

PreferenceFunction::~PreferenceFunction() { }

GetPreferenceFunction::~GetPreferenceFunction() { }

ExtensionFunction::ResponseAction GetPreferenceFunction::Run() {
  std::string pref_key;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &pref_key));
  base::DictionaryValue* details = nullptr;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &details));

  bool incognito = false;
  if (details->HasKey(extensions::preference_api_constants::kIncognitoKey))
    EXTENSION_FUNCTION_VALIDATE(details->GetBoolean(
        extensions::preference_api_constants::kIncognitoKey, &incognito));

  // Check incognito access.
  if (incognito) {
    // Extensions are only allowed to modify incognito preferences if they are
    // enabled in incognito. If the calling browser context is off the record,
    // then the extension must be allowed to run incognito. Otherwise, this
    // could be a spanning mode extension, and we need to check its incognito
    // access.
    if (!browser_context()->IsOffTheRecord() &&
        !include_incognito_information()) {
      return RespondNow(
          Error(extensions::preference_api_constants::kIncognitoErrorMessage));
    }
  }

  // Obtain pref.
  std::string browser_pref;
  APIPermission::ID read_permission = APIPermission::kInvalid;
  APIPermission::ID write_permission = APIPermission::kInvalid;
  EXTENSION_FUNCTION_VALIDATE(
      PrefMapping::GetInstance()->FindBrowserPrefForExtensionPref(
      pref_key, &browser_pref, &read_permission, &write_permission));
  if (!extension()->permissions_data()->HasAPIPermission(read_permission))
    return RespondNow(
        Error(extensions::preference_api_constants::kPermissionErrorMessage,
              pref_key));

  Profile* profile = Profile::FromBrowserContext(browser_context());
  const PrefService* prefs =
      incognito ? profile->GetOffTheRecordPrefs() : profile->GetPrefs();
  const PrefService::Preference* pref = prefs->FindPreference(browser_pref);
  CHECK(pref);

  auto result = std::make_unique<base::DictionaryValue>();

  // Retrieve level of control.
  std::string level_of_control =
      extensions::preference_helpers::GetLevelOfControl(
          profile, extension_id(), browser_pref, incognito);
  result->SetString(extensions::preference_api_constants::kLevelOfControl,
                    level_of_control);

  // Retrieve pref value.
  PrefTransformerInterface* transformer =
      PrefMapping::GetInstance()->FindTransformerForBrowserPref(browser_pref);
  std::unique_ptr<base::Value> transformed_value =
      transformer->BrowserToExtensionPref(pref->GetValue());
  if (!transformed_value) {
    // TODO(devlin): Can this happen?  When?  Should it be an error, or a bad
    // message?
    LOG(ERROR) << ErrorUtils::FormatErrorMessage(kConversionErrorMessage,
                                                 pref->name());
    return RespondNow(Error(kUnknownErrorDoNotUse));
  }
  result->Set(extensions::preference_api_constants::kValue,
              std::move(transformed_value));

  // Retrieve incognito status.
  if (incognito) {
    ExtensionPrefs* ep = ExtensionPrefs::Get(browser_context());
    result->SetBoolean(extensions::preference_api_constants::kIncognitoSpecific,
                       ep->HasIncognitoPrefValue(browser_pref));
  }

  return RespondNow(OneArgument(std::move(result)));
}

SetPreferenceFunction::~SetPreferenceFunction() {}

ExtensionFunction::ResponseAction SetPreferenceFunction::Run() {
  std::string pref_key;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &pref_key));
  base::DictionaryValue* details = nullptr;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &details));

  base::Value* value = nullptr;
  EXTENSION_FUNCTION_VALIDATE(
      details->Get(extensions::preference_api_constants::kValue, &value));

  ExtensionPrefsScope scope = kExtensionPrefsScopeRegular;
  if (details->HasKey(extensions::preference_api_constants::kScopeKey)) {
    std::string scope_str;
    EXTENSION_FUNCTION_VALIDATE(details->GetString(
        extensions::preference_api_constants::kScopeKey, &scope_str));

    EXTENSION_FUNCTION_VALIDATE(
        extensions::preference_helpers::StringToScope(scope_str, &scope));
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
      return RespondNow(
          Error(extensions::preference_api_constants::kIncognitoErrorMessage));
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
      !profile->HasOffTheRecordProfile()) {
    return RespondNow(Error(extensions::preference_api_constants::
                                kIncognitoSessionOnlyErrorMessage));
  }

  // Obtain pref.
  std::string browser_pref;
  APIPermission::ID read_permission = APIPermission::kInvalid;
  APIPermission::ID write_permission = APIPermission::kInvalid;
  EXTENSION_FUNCTION_VALIDATE(
      PrefMapping::GetInstance()->FindBrowserPrefForExtensionPref(
      pref_key, &browser_pref, &read_permission, &write_permission));
  if (!extension()->permissions_data()->HasAPIPermission(write_permission))
    return RespondNow(
        Error(extensions::preference_api_constants::kPermissionErrorMessage,
              pref_key));

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
    return RespondNow(Error(error));
  }
  EXTENSION_FUNCTION_VALIDATE(browser_pref_value->type() == pref->GetType());

  // Validate also that the stored value can be converted back by the
  // transformer.
  std::unique_ptr<base::Value> extension_pref_value(
      transformer->BrowserToExtensionPref(browser_pref_value.get()));
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

  preference_api->SetExtensionControlledPref(
      extension_id(), browser_pref, scope,
      base::Value::FromUniquePtrValue(std::move(browser_pref_value)));

  return RespondNow(NoArguments());
}

ClearPreferenceFunction::~ClearPreferenceFunction() { }

ExtensionFunction::ResponseAction ClearPreferenceFunction::Run() {
  std::string pref_key;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &pref_key));
  base::DictionaryValue* details = nullptr;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &details));

  ExtensionPrefsScope scope = kExtensionPrefsScopeRegular;
  if (details->HasKey(extensions::preference_api_constants::kScopeKey)) {
    std::string scope_str;
    EXTENSION_FUNCTION_VALIDATE(details->GetString(
        extensions::preference_api_constants::kScopeKey, &scope_str));

    EXTENSION_FUNCTION_VALIDATE(
        extensions::preference_helpers::StringToScope(scope_str, &scope));
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
  APIPermission::ID read_permission = APIPermission::kInvalid;
  APIPermission::ID write_permission = APIPermission::kInvalid;
  EXTENSION_FUNCTION_VALIDATE(
      PrefMapping::GetInstance()->FindBrowserPrefForExtensionPref(
      pref_key, &browser_pref, &read_permission, &write_permission));
  if (!extension()->permissions_data()->HasAPIPermission(write_permission))
    return RespondNow(
        Error(extensions::preference_api_constants::kPermissionErrorMessage,
              pref_key));

  PreferenceAPI::Get(browser_context())
      ->RemoveExtensionControlledPref(extension_id(), browser_pref, scope);
  return RespondNow(NoArguments());
}

}  // namespace extensions
