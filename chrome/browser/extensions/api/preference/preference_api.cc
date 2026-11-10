// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/preference/preference_api.h"

#include <stddef.h>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string_view>
#include <utility>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/pref_mapping.h"
#include "chrome/browser/extensions/preference/preference_helpers.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "extensions/browser/api/content_settings/content_settings_service.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_prefs_helper.h"
#include "extensions/browser/extension_prefs_helper_factory.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/pref_transformer_interface.h"
#include "extensions/common/api/types.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "media/media_buildflags.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"

using extensions::mojom::APIPermissionID;

namespace extensions {

namespace {

using extensions::api::types::ChromeSettingScope;

constexpr char kConversionErrorMessage[] =
    "Internal error: Stored value for preference '*' cannot be converted "
    "properly.";
constexpr char kPermissionErrorMessage[] =
    "You do not have permission to access the preference '*'. "
    "Be sure to declare in your manifest what permissions you need.";
constexpr char kNotControllableErrorMessage[] =
    "Cannot set the '*' preference because it is not controllable by "
    "extensions.";
constexpr char kIncognitoKey[] = "incognito";
constexpr char kScopeKey[] = "scope";
constexpr char kIncognitoSpecific[] = "incognitoSpecific";
constexpr char kLevelOfControl[] = "levelOfControl";
constexpr char kValue[] = "value";

bool StringToScope(const std::string& s, ChromeSettingScope& scope) {
  scope = extensions::api::types::ParseChromeSettingScope(s);
  return scope != ChromeSettingScope::kNone;
}

constexpr std::pair<ChromeSettingScope, const char*> kPersistedScopes[] = {
    {ChromeSettingScope::kRegular, pref_names::kPrefPreferences},
    {ChromeSettingScope::kRegularOnly, pref_names::kPrefRegularOnlyPreferences},
    {ChromeSettingScope::kIncognitoPersistent,
     pref_names::kPrefIncognitoPreferences},
};

// Collects all browser preference keys that have a persisted
// extension-controlled value in `ExtensionPrefs` across any scope. Used on
// `PreferenceAPI` startup to scrub values that are no longer modifiable and
// start observing modifiability for those that remain active.
std::set<std::string, std::less<>> GetPersistedExtensionControlledPrefs(
    ExtensionPrefs* extension_prefs) {
  std::set<std::string, std::less<>> persisted_prefs;
  for (const auto& extension_id : extension_prefs->GetExtensions()) {
    for (const auto& [_, scope_pref] : kPersistedScopes) {
      const base::DictValue* prefs_dict =
          extension_prefs->ReadPrefAsDict(extension_id, scope_pref);
      if (!prefs_dict) {
        continue;
      }
      for (const auto [pref_key, unused] : *prefs_dict) {
        persisted_prefs.insert(pref_key);
      }
    }
  }
  return persisted_prefs;
}

void ClearExtensionControlledPref(Profile* profile,
                                  const std::string& browser_pref) {
  auto* prefs_helper = ExtensionPrefsHelper::Get(profile);
  auto* extension_prefs = ExtensionPrefs::Get(profile);
  if (!prefs_helper || !extension_prefs) {
    return;
  }
  for (const auto& extension_id : extension_prefs->GetExtensions()) {
    for (const auto& [scope, scope_pref] : kPersistedScopes) {
      const base::DictValue* prefs_dict =
          extension_prefs->ReadPrefAsDict(extension_id, scope_pref);
      if (prefs_dict && prefs_dict->contains(browser_pref)) {
        prefs_helper->RemoveExtensionControlledPref(extension_id, browser_pref,
                                                    scope);
      }
    }
    prefs_helper->RemoveExtensionControlledPref(
        extension_id, browser_pref, ChromeSettingScope::kIncognitoSessionOnly);
  }
}

}  // namespace

PreferenceEventRouter::PreferenceEventRouter(Profile* profile)
    : profile_(profile) {
  registrar_.Init(profile_->GetPrefs());
  for (const auto& pref : PrefMapping::GetMappings()) {
    registrar_.Add(
        pref.browser_pref,
        base::BindRepeating(&PreferenceEventRouter::OnPrefChanged,
                            base::Unretained(this), registrar_.prefs()));
  }
  CHECK(!profile_->IsOffTheRecord(), base::NotFatalUntil::M161);
  observed_profiles_.AddObservation(profile_.get());
  if (profile->HasPrimaryOTRProfile()) {
    OnOffTheRecordProfileCreated(
        profile->GetPrimaryOTRProfile(/*create_if_needed=*/true));
  } else {
    ObserveOffTheRecordPrefs(profile->GetReadOnlyOffTheRecordPrefs());
  }
}

PreferenceEventRouter::~PreferenceEventRouter() = default;

void PreferenceEventRouter::OnPrefChanged(PrefService* pref_service,
                                          const std::string& browser_pref) {
  if (suppress_pref_changed_) {
    return;
  }
  bool incognito = (pref_service != profile_->GetPrefs());

  std::string event_name;
  APIPermissionID permission = APIPermissionID::kInvalid;
  bool rv = PrefMapping::GetInstance()->FindEventForBrowserPref(
      browser_pref, &event_name, &permission);
  CHECK(rv, base::NotFatalUntil::M161);

  base::ListValue args;
  const PrefService::Preference* pref =
      pref_service->FindPreference(browser_pref);
  CHECK(pref);
  PrefTransformerInterface* transformer =
      PrefMapping::GetInstance()->FindTransformerForBrowserPref(browser_pref);
  CHECK(transformer);
  std::optional<base::Value> transformed_value =
      transformer->BrowserToExtensionPref(*pref->GetValue(), incognito);
  if (!transformed_value) {
    LOG(ERROR) << ErrorUtils::FormatErrorMessage(kConversionErrorMessage,
                                                 pref->name());
    return;
  }

  base::DictValue dict;
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

void PreferenceEventRouter::OnPrefModifiabilityChanged(
    const std::string& browser_pref) {
  PrefTransformerInterface* transformer =
      PrefMapping::GetInstance()->FindTransformerForBrowserPref(browser_pref);
  CHECK(transformer);

  if (!transformer->IsExtensionModifiable(profile_)) {
    base::AutoReset<bool> suppress(&suppress_pref_changed_, true);
    ClearExtensionControlledPref(profile_, browser_pref);
  }

  // Dispatch `OnPrefChanged()` once per `PrefService` (regular and incognito)
  // for the final value and updated `levelOfControl`, unless a higher-priority
  // store controls the preference (where neither the effective value nor
  // `levelOfControl` changed).
  for (PrefService* prefs :
       {registrar_.prefs(), incognito_registrar_->prefs()}) {
    const PrefService::Preference* pref = prefs->FindPreference(browser_pref);
    if (pref && pref->IsExtensionModifiable()) {
      OnPrefChanged(prefs, browser_pref);
    }
  }
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
  // Preferences.
  ExtensionFunctionRegistry& registry =
      ExtensionFunctionRegistry::GetInstance();
  registry.RegisterFunction<GetPreferenceFunction>();
  registry.RegisterFunction<SetPreferenceFunction>();
  registry.RegisterFunction<ClearPreferenceFunction>();

  PrefMapping* pref_mapping = PrefMapping::GetInstance();
  const std::set<std::string, std::less<>> persisted_prefs =
      GetPersistedExtensionControlledPrefs(ExtensionPrefs::Get(profile_));

  for (const auto& pref : PrefMapping::GetMappings()) {
    std::string event_name;
    APIPermissionID permission = APIPermissionID::kInvalid;
    bool rv = pref_mapping->FindEventForBrowserPref(pref.browser_pref,
                                                    &event_name, &permission);
    CHECK(rv, base::NotFatalUntil::M161);
    EventRouter::Get(profile_)->RegisterObserver(this, event_name);

    if (persisted_prefs.contains(pref.browser_pref)) {
      PrefTransformerInterface* transformer =
          pref_mapping->FindTransformerForBrowserPref(pref.browser_pref);
      CHECK(transformer);
      if (!transformer->IsExtensionModifiable(profile_)) {
        ClearExtensionControlledPref(profile_, pref.browser_pref);
      } else {
        ObservePrefModifiability(pref.browser_pref);
      }
    }
  }
  content_settings_store()->AddObserver(this);
}

PreferenceAPI::~PreferenceAPI() {
  RemoveModifiabilityObservers();
}

void PreferenceAPI::Shutdown() {
  shutting_down_ = true;
  RemoveModifiabilityObservers();
  EventRouter::Get(profile_)->UnregisterObserver(this);
  if (!ExtensionPrefs::Get(profile_)->extensions_disabled()) {
    ClearIncognitoSessionOnlyContentSettings();
  }
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
    for (const auto& pref : PrefMapping::GetMappings()) {
      ObservePrefModifiability(pref.browser_pref);
    }
  }
}

void PreferenceAPI::ObservePrefModifiability(const std::string& browser_pref) {
  if (shutting_down_ || observed_modifiability_prefs_.contains(browser_pref)) {
    return;
  }
  PrefTransformerInterface* transformer =
      PrefMapping::GetInstance()->FindTransformerForBrowserPref(browser_pref);
  CHECK(transformer);
  transformer->AddObserver(profile_, browser_pref, this);
  observed_modifiability_prefs_.insert(browser_pref);
}

void PreferenceAPI::OnPrefModifiabilityChanged(
    const std::string& browser_pref) {
  // When an `onChange` listener is registered, delegate to
  // `PreferenceEventRouter` so clearing extension values and dispatching the
  // resulting `onChange` update happen together (coalescing intermediate
  // `OnPrefChanged()` callbacks). Otherwise, clear unmodifiable values directly
  // without instantiating `PreferenceEventRouter`.
  if (preference_event_router_) {
    preference_event_router_->OnPrefModifiabilityChanged(browser_pref);
    return;
  }
  PrefTransformerInterface* transformer =
      PrefMapping::GetInstance()->FindTransformerForBrowserPref(browser_pref);
  CHECK(transformer);
  if (!transformer->IsExtensionModifiable(profile_)) {
    ClearExtensionControlledPref(profile_, browser_pref);
  }
}

void PreferenceAPI::RemoveModifiabilityObservers() {
  for (const std::string& browser_pref : observed_modifiability_prefs_) {
    PrefTransformerInterface* transformer =
        PrefMapping::GetInstance()->FindTransformerForBrowserPref(browser_pref);
    CHECK(transformer);
    transformer->RemoveObserver(profile_, this);
  }
  observed_modifiability_prefs_.clear();
}

void PreferenceAPI::OnContentSettingChanged(const ExtensionId& extension_id,
                                            bool incognito) {
  if (incognito) {
    ExtensionPrefs::Get(profile_)->UpdateExtensionPref(
        extension_id, pref_names::kPrefIncognitoContentSettings,
        base::Value(content_settings_store()->GetSettingsForExtension(
            extension_id, ChromeSettingScope::kIncognitoPersistent)));
  } else {
    ExtensionPrefs::Get(profile_)->UpdateExtensionPref(
        extension_id, pref_names::kPrefContentSettings,
        base::Value(content_settings_store()->GetSettingsForExtension(
            extension_id, ChromeSettingScope::kRegular)));
  }
}

void PreferenceAPI::ClearIncognitoSessionOnlyContentSettings() {
  for (const auto& id : ExtensionPrefs::Get(profile_)->GetExtensions()) {
    content_settings_store()->ClearContentSettingsForExtension(
        id, ChromeSettingScope::kIncognitoSessionOnly);
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
  DependsOn(ExtensionPrefsHelperFactory::GetInstance());
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
  if (std::optional<bool> result = details.GetDict().FindBool(kIncognitoKey)) {
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
  if (!extension()->permissions_data()->HasAPIPermission(read_permission)) {
    return RespondNow(Error(kPermissionErrorMessage, pref_key));
  }

  Profile* profile = Profile::FromBrowserContext(browser_context());

  PrefService* prefs =
      extensions::preference_helpers::GetProfilePrefService(profile, incognito);

  const PrefService::Preference* pref = prefs->FindPreference(browser_pref);
  CHECK(pref);

  // Retrieve level of control.
  std::string level_of_control =
      extensions::preference_helpers::GetLevelOfControl(
          profile, extension_id(), browser_pref, incognito);

  base::DictValue result;
  ProduceGetResult(&result, pref->GetValue(), level_of_control, browser_pref,
                   incognito);

  return RespondNow(WithArguments(std::move(result)));
}

void GetPreferenceFunction::ProduceGetResult(
    base::DictValue* result,
    const base::Value* pref_value,
    const std::string& level_of_control,
    const std::string& browser_pref,
    bool incognito) {
  PrefTransformerInterface* transformer =
      PrefMapping::GetInstance()->FindTransformerForBrowserPref(browser_pref);
  CHECK(transformer);
  std::optional<base::Value> transformed_value =
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

SetPreferenceFunction::~SetPreferenceFunction() = default;

ExtensionFunction::ResponseAction SetPreferenceFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 2);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_string());
  EXTENSION_FUNCTION_VALIDATE(args()[1].is_dict());

  std::string pref_key = args()[0].GetString();
  const base::DictValue& details = args()[1].GetDict();

  const base::Value* value = details.Find(kValue);
  EXTENSION_FUNCTION_VALIDATE(value);

  ChromeSettingScope scope = ChromeSettingScope::kRegular;
  if (const std::string* scope_str = details.FindString(kScopeKey)) {
    EXTENSION_FUNCTION_VALIDATE(StringToScope(*scope_str, scope));
  }

  // Check incognito scope.
  bool incognito = scope == ChromeSettingScope::kIncognitoPersistent ||
                   scope == ChromeSettingScope::kIncognitoSessionOnly;
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
  if (scope == ChromeSettingScope::kIncognitoSessionOnly &&
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
  if (!extension()->permissions_data()->HasAPIPermission(write_permission)) {
    return RespondNow(Error(kPermissionErrorMessage, pref_key));
  }

  // As 3PCs are globally blocked in incognito re-allowing them is not
  // supported, so error out.
  if (incognito && browser_pref == prefs::kCookieControlsMode &&
      value->GetBool()) {
    return RespondNow(
        Error(extension_misc::kCookiesAllowedIncognitoErrorMessage));
  }

  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context());
  const PrefService::Preference* pref =
      prefs->pref_service()->FindPreference(browser_pref);
  CHECK(pref);

  // Validate new value.
  PrefTransformerInterface* transformer =
      PrefMapping::GetInstance()->FindTransformerForBrowserPref(browser_pref);
  CHECK(transformer);
  if (!transformer->IsExtensionModifiable(profile)) {
    return RespondNow(Error(kNotControllableErrorMessage, pref_key));
  }
  std::string error;
  bool bad_message = false;
  std::optional<base::Value> browser_pref_value =
      transformer->ExtensionToBrowserPref(*value, error, bad_message);
  if (!browser_pref_value) {
    EXTENSION_FUNCTION_VALIDATE(!bad_message);
    return RespondNow(Error(std::move(error)));
  }
  EXTENSION_FUNCTION_VALIDATE(browser_pref_value->type() == pref->GetType());

  // Validate also that the stored value can be converted back by the
  // transformer.
  std::optional<base::Value> extension_pref_value =
      transformer->BrowserToExtensionPref(*browser_pref_value, incognito);
  EXTENSION_FUNCTION_VALIDATE(extension_pref_value);

  auto* prefs_helper = ExtensionPrefsHelper::Get(browser_context());

  // Set the new Autofill prefs if the extension sets the deprecated pref in
  // order to maintain backward compatibility in the extensions preference API.
  // TODO(crbug.com/40587768): Remove this once the deprecated pref is retired.
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
  // See crbug.com/40681445 for more background.
  //
  // TODO(crbug.com/40681445): Consider extending
  // chrome.privacy.services.safeBrowsingEnabled to a three-state enum.
  if (prefs::kSafeBrowsingEnabled == browser_pref) {
    prefs_helper->SetExtensionControlledPref(extension_id(),
                                             prefs::kSafeBrowsingEnhanced,
                                             scope, base::Value(false));
  }

  if (auto* preference_api = PreferenceAPI::Get(browser_context())) {
    preference_api->ObservePrefModifiability(browser_pref);
  }

  prefs_helper->SetExtensionControlledPref(extension_id(), browser_pref, scope,
                                           browser_pref_value->Clone());

  return RespondNow(NoArguments());
}

ClearPreferenceFunction::~ClearPreferenceFunction() = default;

ExtensionFunction::ResponseAction ClearPreferenceFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 2);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_string());
  EXTENSION_FUNCTION_VALIDATE(args()[1].is_dict());

  std::string pref_key = args()[0].GetString();
  const base::DictValue& details = args()[1].GetDict();

  ChromeSettingScope scope = ChromeSettingScope::kRegular;
  if (const std::string* scope_str = details.FindString(kScopeKey)) {
    EXTENSION_FUNCTION_VALIDATE(StringToScope(*scope_str, scope));
  }

  // Check incognito scope.
  bool incognito = scope == ChromeSettingScope::kIncognitoPersistent ||
                   scope == ChromeSettingScope::kIncognitoSessionOnly;
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
  if (!extension()->permissions_data()->HasAPIPermission(write_permission)) {
    return RespondNow(Error(kPermissionErrorMessage, pref_key));
  }

  auto* prefs_helper = ExtensionPrefsHelper::Get(browser_context());

  prefs_helper->RemoveExtensionControlledPref(extension_id(), browser_pref,
                                              scope);

  // Whenever an extension clears the |kSafeBrowsingEnabled| preference,
  // it must also clear |kSafeBrowsingEnhanced|. See crbug.com/40681445 for
  // more background.
  //
  // TODO(crbug.com/40681445): Consider extending
  // chrome.privacy.services.safeBrowsingEnabled to a three-state enum.
  if (prefs::kSafeBrowsingEnabled == browser_pref) {
    prefs_helper->RemoveExtensionControlledPref(
        extension_id(), prefs::kSafeBrowsingEnhanced, scope);
  }
  return RespondNow(NoArguments());
}

}  // namespace extensions
