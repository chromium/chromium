// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/preference/preference_api.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/preference/preference_helpers.h"
#include "chrome/browser/extensions/api/proxy/proxy_api.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/pref_mapping.h"
#include "chrome/browser/extensions/pref_transformer_interface.h"
#include "chrome/browser/prefetch/pref_names.h"
#include "chrome/browser/prefetch/prefetch_prefs.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "extensions/browser/api/content_settings/content_settings_service.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_prefs_helper.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "media/media_buildflags.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/chromeos/extensions/controlled_pref_mapping.h"
#include "chromeos/startup/browser_params_proxy.h"
#endif

using extensions::mojom::APIPermissionID;

namespace extensions {

namespace {

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

// Transform the thirdPartyCookiesAllowed extension api to CookieControlsMode
// enum values.
class CookieControlsModeTransformer : public PrefTransformerInterface {
  using CookieControlsMode = content_settings::CookieControlsMode;

 public:
  absl::optional<base::Value> ExtensionToBrowserPref(
      const base::Value& extension_pref,
      std::string& error,
      bool& bad_message) override {
    bool third_party_cookies_allowed = extension_pref.GetBool();
    return base::Value(static_cast<int>(
        third_party_cookies_allowed ? CookieControlsMode::kOff
                                    : CookieControlsMode::kBlockThirdParty));
  }

  absl::optional<base::Value> BrowserToExtensionPref(
      const base::Value& browser_pref,
      bool is_incognito_profile) override {
    auto cookie_control_mode =
        static_cast<CookieControlsMode>(browser_pref.GetInt());

    bool third_party_cookies_allowed =
        cookie_control_mode == content_settings::CookieControlsMode::kOff ||
        (!is_incognito_profile &&
         cookie_control_mode == CookieControlsMode::kIncognitoOnly);

    return base::Value(third_party_cookies_allowed);
  }
};

class NetworkPredictionTransformer : public PrefTransformerInterface {
 public:
  absl::optional<base::Value> ExtensionToBrowserPref(
      const base::Value& extension_pref,
      std::string& error,
      bool& bad_message) override {
    if (!extension_pref.is_bool()) {
      DCHECK(false) << "Preference not found.";
    } else if (extension_pref.GetBool()) {
      return base::Value(
          static_cast<int>(prefetch::NetworkPredictionOptions::kDefault));
    }
    return base::Value(
        static_cast<int>(prefetch::NetworkPredictionOptions::kDisabled));
  }

  absl::optional<base::Value> BrowserToExtensionPref(
      const base::Value& browser_pref,
      bool is_incognito_profile) override {
    prefetch::NetworkPredictionOptions value =
        prefetch::NetworkPredictionOptions::kDefault;
    if (browser_pref.is_int()) {
      value = static_cast<prefetch::NetworkPredictionOptions>(
          browser_pref.GetInt());
    }
    return base::Value(value != prefetch::NetworkPredictionOptions::kDisabled);
  }
};

class ProtectedContentEnabledTransformer : public PrefTransformerInterface {
 public:
  absl::optional<base::Value> ExtensionToBrowserPref(
      const base::Value& extension_pref,
      std::string& error,
      bool& bad_message) override {
    bool protected_identifier_allowed = extension_pref.GetBool();
    return base::Value(static_cast<int>(protected_identifier_allowed
                                            ? CONTENT_SETTING_ALLOW
                                            : CONTENT_SETTING_BLOCK));
  }

  absl::optional<base::Value> BrowserToExtensionPref(
      const base::Value& browser_pref,
      bool is_incognito_profile) override {
    auto protected_identifier_mode =
        static_cast<ContentSetting>(browser_pref.GetInt());
    return base::Value(protected_identifier_mode == CONTENT_SETTING_ALLOW);
  }
};

// Return error when extensions try to enable a Privacy Sandbox API.
class PrivacySandboxTransformer : public PrefTransformerInterface {
 public:
  absl::optional<base::Value> ExtensionToBrowserPref(
      const base::Value& extension_pref,
      std::string& error,
      bool& bad_message) override {
    if (!extension_pref.is_bool()) {
      bad_message = true;
      return absl::nullopt;
    }

    if (extension_pref.GetBool()) {
      error = "Extensions aren’t allowed to enable Privacy Sandbox APIs.";
      return absl::nullopt;
    }

    return extension_pref.Clone();
  }

  // Default behaviour
  absl::optional<base::Value> BrowserToExtensionPref(
      const base::Value& browser_pref,
      bool is_incognito_profile) override {
    return browser_pref.Clone();
  }
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
  for (const auto& pref : PrefMapping::GetMappings()) {
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

  base::Value::List args;
  PrefTransformerInterface* transformer =
      PrefMapping::GetInstance()->FindTransformerForBrowserPref(browser_pref);

  absl::optional<base::Value> transformed_value =
      transformer->BrowserToExtensionPref(opt_value.value(), incognito);
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
      profile_, histogram_value, event_name, std::move(args), permission,
      incognito, browser_pref, control_state);
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

  base::Value::List args;
  const PrefService::Preference* pref =
      pref_service->FindPreference(browser_pref);
  CHECK(pref);
  PrefTransformerInterface* transformer =
      PrefMapping::GetInstance()->FindTransformerForBrowserPref(browser_pref);
  absl::optional<base::Value> transformed_value =
      transformer->BrowserToExtensionPref(*pref->GetValue(), incognito);
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
      profile_, histogram_value, event_name, std::move(args), permission,
      incognito, browser_pref);
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
  for (const auto& pref : PrefMapping::GetMappings()) {
    incognito_registrar_->Add(
        pref.browser_pref,
        base::BindRepeating(&PreferenceEventRouter::OnPrefChanged,
                            base::Unretained(this),
                            incognito_registrar_->prefs()));
  }
}

PreferenceAPI::PreferenceAPI(content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)) {
  PrefMapping* pref_mapping = PrefMapping::GetInstance();

  // TODO(dbertoni): Only register the transformers once. We need a better
  // place to do this and to only do it once. This will allow getting rid of
  // the HasPrefTransformer API. Also, the ProxyPrefTransformer needs to be
  // registered in somewhere else. This will happen in a follow-on CL.
  if (!pref_mapping->HasPrefTransformer(prefs::kCookieControlsMode)) {
    pref_mapping->RegisterPrefTransformer(
        prefs::kCookieControlsMode,
        std::make_unique<CookieControlsModeTransformer>());
    pref_mapping->RegisterPrefTransformer(
        proxy_config::prefs::kProxy, std::make_unique<ProxyPrefTransformer>());
    pref_mapping->RegisterPrefTransformer(
        prefetch::prefs::kNetworkPredictionOptions,
        std::make_unique<NetworkPredictionTransformer>());
    pref_mapping->RegisterPrefTransformer(
        prefs::kProtectedContentDefault,
        std::make_unique<ProtectedContentEnabledTransformer>());
  }

  if (!pref_mapping->HasPrefTransformer(
          prefs::kPrivacySandboxM1TopicsEnabled)) {
    pref_mapping->RegisterPrefTransformer(
        prefs::kPrivacySandboxM1TopicsEnabled,
        std::make_unique<PrivacySandboxTransformer>());
    pref_mapping->RegisterPrefTransformer(
        prefs::kPrivacySandboxM1FledgeEnabled,
        std::make_unique<PrivacySandboxTransformer>());
    pref_mapping->RegisterPrefTransformer(
        prefs::kPrivacySandboxM1AdMeasurementEnabled,
        std::make_unique<PrivacySandboxTransformer>());
  }

  for (const auto& pref : PrefMapping::GetMappings()) {
    std::string event_name;
    APIPermissionID permission = APIPermissionID::kInvalid;
    bool rv = pref_mapping->FindEventForBrowserPref(pref.browser_pref,
                                                    &event_name, &permission);
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
  if (!ExtensionPrefs::Get(profile_)->extensions_disabled())
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
    ExtensionPrefs::Get(profile_)->UpdateExtensionPref(
        extension_id, pref_names::kPrefIncognitoContentSettings,
        base::Value(content_settings_store()->GetSettingsForExtension(
            extension_id, kExtensionPrefsScopeIncognitoPersistent)));
  } else {
    ExtensionPrefs::Get(profile_)->UpdateExtensionPref(
        extension_id, pref_names::kPrefContentSettings,
        base::Value(content_settings_store()->GetSettingsForExtension(
            extension_id, kExtensionPrefsScopeRegular)));
  }
}

void PreferenceAPI::ClearIncognitoSessionOnlyContentSettings() {
  for (const auto& id : ExtensionPrefs::Get(profile_)->GetExtensions()) {
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

// Auxiliary function to build an InspectorInfoPtr to create a Deprecation
// Issue when the deprecated privacySandboxEnabled API is used
// TODO(b/263568309): Remove this once the deprecated API is retired.
blink::mojom::InspectorIssueInfoPtr
BuildPrivacySandboxDeprecationInspectorIssueInfo(const GURL& source_url) {
  auto issue_info = blink::mojom::InspectorIssueInfo::New();
  issue_info->code = blink::mojom::InspectorIssueCode::kDeprecationIssue;
  issue_info->details = blink::mojom::InspectorIssueDetails::New();
  auto deprecation_details = blink::mojom::DeprecationIssueDetails::New();
  deprecation_details->type =
      blink::mojom::DeprecationIssueType::kPrivacySandboxExtensionsAPI;
  auto affected_location = blink::mojom::AffectedLocation::New();
  affected_location->url = source_url.spec();
  deprecation_details->affected_location = std::move(affected_location);
  issue_info->details->deprecation_issue_details =
      std::move(deprecation_details);

  return issue_info;
}

GetPreferenceFunction::~GetPreferenceFunction() = default;

ExtensionFunction::ResponseAction GetPreferenceFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 2);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_string());
  EXTENSION_FUNCTION_VALIDATE(args()[1].is_dict());

  const std::string& pref_key = args()[0].GetString();
  const base::Value& details = args()[1];

  bool incognito = false;
  if (absl::optional<bool> result = details.GetDict().FindBool(kIncognitoKey)) {
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

  // Deprecation issue to developers in the issues tab in Chrome DevTools that
  // the API chrome.privacy.websites.privacySandboxEnabled is being deprecated.
  // TODO(b/263568309): Remove this once the deprecated API is retired.
  if (prefs::kPrivacySandboxApisEnabled == browser_pref) {
    ReportInspectorIssue(
        BuildPrivacySandboxDeprecationInspectorIssueInfo(source_url()));
  }

  PrefService* prefs =
      extensions::preference_helpers::GetProfilePrefService(profile, incognito);

  const PrefService::Preference* pref = prefs->FindPreference(browser_pref);
  CHECK(pref);

  // Retrieve level of control.
  std::string level_of_control =
      extensions::preference_helpers::GetLevelOfControl(
          profile, extension_id(), browser_pref, incognito);

  base::Value::Dict result;
  ProduceGetResult(&result, pref->GetValue(), level_of_control, browser_pref,
                   incognito);

  return RespondNow(OneArgument(base::Value(std::move(result))));
}

void GetPreferenceFunction::ProduceGetResult(
    base::Value::Dict* result,
    const base::Value* pref_value,
    const std::string& level_of_control,
    const std::string& browser_pref,
    bool incognito) {
  PrefTransformerInterface* transformer =
      PrefMapping::GetInstance()->FindTransformerForBrowserPref(browser_pref);
  absl::optional<base::Value> transformed_value =
      transformer->BrowserToExtensionPref(*pref_value, incognito);
  if (!transformed_value) {
    // TODO(devlin): Can this happen?  When?  Should it be an error, or a bad
    // message?
    LOG(ERROR) << ErrorUtils::FormatErrorMessage(kConversionErrorMessage,
                                                 browser_pref);
    return;
  }

  result->Set(kValue, std::move(*transformed_value));
  result->Set(kLevelOfControl, level_of_control);

  // Retrieve incognito status.
  if (incognito) {
    ExtensionPrefs* ep = ExtensionPrefs::Get(browser_context());
    result->Set(kIncognitoSpecific, ep->HasIncognitoPrefValue(browser_pref));
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
  if (absl::optional<bool> result = details.GetDict().FindBool(kIncognitoKey)) {
    incognito = *result;
  }

  ::base::Value* pref_value = &opt_value.value();

  std::string level_of_control;
  level_of_control =
      extensions::preference_helpers::GetLevelOfControlWithAshControlState(
          control_state, profile, extension_id(), cached_browser_pref_,
          incognito);

  base::Value::Dict result;

  ProduceGetResult(&result, pref_value, level_of_control, cached_browser_pref_,
                   incognito);

  Respond(OneArgument(base::Value(std::move(result))));
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
  absl::optional<base::Value> browser_pref_value =
      transformer->ExtensionToBrowserPref(*value, error, bad_message);
  if (!browser_pref_value) {
    EXTENSION_FUNCTION_VALIDATE(!bad_message);
    return RespondNow(Error(std::move(error)));
  }
  EXTENSION_FUNCTION_VALIDATE(browser_pref_value->type() == pref->GetType());

  // Validate also that the stored value can be converted back by the
  // transformer.
  absl::optional<base::Value> extension_pref_value =
      transformer->BrowserToExtensionPref(*browser_pref_value, incognito);
  EXTENSION_FUNCTION_VALIDATE(extension_pref_value);

  auto* prefs_helper = ExtensionPrefsHelper::Get(browser_context());

  // Set the new Autofill prefs if the extension sets the deprecated pref in
  // order to maintain backward compatibility in the extensions preference API.
  // TODO(crbug.com/870328): Remove this once the deprecated pref is retired.
  if (autofill::prefs::kAutofillEnabledDeprecated == browser_pref) {
    // |SetExtensionControlledPref| takes ownership of the base::Value pointer.
    prefs_helper->SetExtensionControlledPref(
        extension_id(), autofill::prefs::kAutofillCreditCardEnabled, scope,
        base::Value(browser_pref_value->GetBool()));
    prefs_helper->SetExtensionControlledPref(
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
    prefs_helper->SetExtensionControlledPref(extension_id(),
                                             prefs::kSafeBrowsingEnhanced,
                                             scope, base::Value(false));
  }

  // Deprecation issue to developers in the issues tab in Chrome DevTools that
  // the API chrome.privacy.websites.privacySandboxEnabled is being deprecated.
  // TODO(b/263568309): Remove this once the deprecated API is retired.
  if (prefs::kPrivacySandboxApisEnabled == browser_pref) {
    ReportInspectorIssue(
        BuildPrivacySandboxDeprecationInspectorIssueInfo(source_url()));
  }

  // Clear the new Privacy Sandbox APIs if an extension sets to true the
  // deprecated pref |kPrivacySandboxApisEnabled| and set to false the new
  // Privacy Sandbox APIs if an extension sets to false the deprecated pref
  // |kPrivacySandboxApisEnabled| in order to maintain backward compatibility
  // during the migration period.
  // TODO(b/263568309): Remove this once the deprecated API is retired.
  if (prefs::kPrivacySandboxApisEnabled == browser_pref) {
    if (browser_pref_value->GetBool()) {
      prefs_helper->RemoveExtensionControlledPref(
          extension_id(), prefs::kPrivacySandboxM1TopicsEnabled, scope);
      prefs_helper->RemoveExtensionControlledPref(
          extension_id(), prefs::kPrivacySandboxM1FledgeEnabled, scope);
      prefs_helper->RemoveExtensionControlledPref(
          extension_id(), prefs::kPrivacySandboxM1AdMeasurementEnabled, scope);
    } else {
      prefs_helper->SetExtensionControlledPref(
          extension_id(), prefs::kPrivacySandboxM1TopicsEnabled, scope,
          base::Value(false));
      prefs_helper->SetExtensionControlledPref(
          extension_id(), prefs::kPrivacySandboxM1FledgeEnabled, scope,
          base::Value(false));
      prefs_helper->SetExtensionControlledPref(
          extension_id(), prefs::kPrivacySandboxM1AdMeasurementEnabled, scope,
          base::Value(false));
    }
  }

  prefs_helper->SetExtensionControlledPref(extension_id(), browser_pref, scope,
                                           browser_pref_value->Clone());
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (pref_path != crosapi::mojom::PrefPath::kUnknown &&
      prefs_helper->DoesExtensionControlPref(extension_id(), browser_pref,
                                             nullptr)) {
    lacros_service->GetRemote<crosapi::mojom::Prefs>()->SetPref(
        pref_path, std::move(*browser_pref_value),
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

  auto* prefs_helper = ExtensionPrefsHelper::Get(browser_context());

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
  bool did_just_control_pref = prefs_helper->DoesExtensionControlPref(
      extension_id(), browser_pref, nullptr);
#endif

  prefs_helper->RemoveExtensionControlledPref(extension_id(), browser_pref,
                                              scope);

  // Deprecation issue to developers in the issues tab in Chrome DevTools that
  // the API chrome.privacy.websites.privacySandboxEnabled is being deprecated.
  // TODO(b/263568309): Remove this once the deprecated API is retired.
  if (prefs::kPrivacySandboxApisEnabled == browser_pref) {
    ReportInspectorIssue(
        BuildPrivacySandboxDeprecationInspectorIssueInfo(source_url()));
  }

  // Clear the new Privacy Sandbox APIs if an extension clears the deprecated
  // pref |kPrivacySandboxApisEnabled| in order to maintain backward
  // compatibility during the migration period.
  // TODO(b/263568309): Remove this once the deprecated API is retired.
  if (prefs::kPrivacySandboxApisEnabled == browser_pref) {
    prefs_helper->RemoveExtensionControlledPref(
        extension_id(), prefs::kPrivacySandboxM1TopicsEnabled, scope);
    prefs_helper->RemoveExtensionControlledPref(
        extension_id(), prefs::kPrivacySandboxM1FledgeEnabled, scope);
    prefs_helper->RemoveExtensionControlledPref(
        extension_id(), prefs::kPrivacySandboxM1AdMeasurementEnabled, scope);
  }

  // Whenever an extension clears the |kSafeBrowsingEnabled| preference,
  // it must also clear |kSafeBrowsingEnhanced|. See crbug.com/1064722 for
  // more background.
  //
  // TODO(crbug.com/1064722): Consider extending
  // chrome.privacy.services.safeBrowsingEnabled to a three-state enum.
  if (prefs::kSafeBrowsingEnabled == browser_pref) {
    prefs_helper->RemoveExtensionControlledPref(
        extension_id(), prefs::kSafeBrowsingEnhanced, scope);
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
