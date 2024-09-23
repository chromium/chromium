// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Font Settings Extension API implementation.

#include "chrome/browser/extensions/api/font_settings/font_settings_api.h"

#include <stddef.h>

#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/preference/preference_helpers.h"
#include "chrome/browser/font_pref_change_notifier.h"
#include "chrome/browser/font_pref_change_notifier_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/font_settings.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_names_util.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/font_list_async.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_prefs_helper.h"
#include "extensions/browser/extension_prefs_helper_factory.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/api/types.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/mojom/api_permission_id.mojom.h"

#if BUILDFLAG(IS_WIN)
#include "ui/gfx/win/direct_write.h"
#endif  // BUILDFLAG(IS_WIN)

namespace extensions {

namespace fonts = api::font_settings;
using extensions::api::types::ChromeSettingScope;

namespace {

const char kFontIdKey[] = "fontId";
const char kGenericFamilyKey[] = "genericFamily";
const char kLevelOfControlKey[] = "levelOfControl";
const char kDisplayNameKey[] = "displayName";
const char kPixelSizeKey[] = "pixelSize";
const char kScriptKey[] = "script";

const char kSetFromIncognitoError[] =
    "Can't modify regular settings from an incognito context.";

// Gets the font name preference path for |generic_family| and |script|. If
// |script| is NULL, uses prefs::kWebKitCommonScript.
std::string GetFontNamePrefPath(fonts::GenericFamily generic_family_enum,
                                fonts::ScriptCode script_enum) {
  // Format is <prefix-(includes-dot)><family>.<script>
  std::string result;
  size_t prefix_len = strlen(pref_names_util::kWebKitFontPrefPrefix);
  std::string generic_family = fonts::ToString(generic_family_enum);

  // Script codes are 4, dot adds one more for 5.
  result.reserve(prefix_len + generic_family.size() + 5);

  result.append(pref_names_util::kWebKitFontPrefPrefix, prefix_len);
  result.append(fonts::ToString(generic_family_enum));
  result.push_back('.');

  const char* script = fonts::ToString(script_enum);
  if (script[0] == 0)  // Empty string.
    result.append(prefs::kWebKitCommonScript);
  else
    result.append(script);
  return result;
}

void MaybeUnlocalizeFontName(std::string* font_name) {
#if BUILDFLAG(IS_WIN)
  // Try to get the 'us-en' font name. If it is failing, use the first name
  // available.
  std::optional<std::string> localized_font_name =
      gfx::win::RetrieveLocalizedFontName(*font_name, "us-en");
  if (!localized_font_name)
    localized_font_name = gfx::win::RetrieveLocalizedFontName(*font_name, "");

  if (localized_font_name)
    *font_name = std::move(localized_font_name.value());
#endif  // BUILDFLAG(IS_WIN)
}

}  // namespace

// This class observes pref changed events on a profile and dispatches the
// corresponding extension API events to extensions.
class FontSettingsEventRouter {
 public:
  // Constructor for observing pref changed events on `profile`. Stores a
  // pointer to `profile` but does not take ownership. `profile` must be
  // non-NULL and remain alive for the lifetime of the instance.
  explicit FontSettingsEventRouter(Profile* profile);

  FontSettingsEventRouter(const FontSettingsEventRouter&) = delete;
  FontSettingsEventRouter& operator=(const FontSettingsEventRouter&) = delete;

  virtual ~FontSettingsEventRouter();

 private:
  // Observes browser pref `pref_name`. When a change is observed, dispatches
  // event `event_name` to extensions. A JavaScript object is passed to the
  // extension event function with the new value of the pref in property `key`.
  void AddPrefToObserve(const char* pref_name,
                        events::HistogramValue histogram_value,
                        const char* event_name,
                        const char* key);

  // Decodes a preference change for a font family map and invokes
  // OnFontNamePrefChange with the right parameters.
  void OnFontFamilyMapPrefChanged(const std::string& pref_name);

  // Dispatches a changed event for the font setting for `generic_family` and
  // `script` to extensions. The new value of the setting is the value of
  // browser pref `pref_name`.
  void OnFontNamePrefChanged(const std::string& pref_name,
                             const std::string& generic_family,
                             const std::string& script);

  // Dispatches the setting changed event `event_name` to extensions. The new
  // value of the setting is the value of browser pref `pref_name`. This value
  // is passed in the JavaScript object argument to the extension event function
  // under the key `key`.
  void OnFontPrefChanged(events::HistogramValue histogram_value,
                         const std::string& event_name,
                         const std::string& key,
                         const std::string& pref_name);

  // Manages pref observation registration.
  PrefChangeRegistrar registrar_;
  FontPrefChangeNotifier::Registrar font_change_registrar_;

  // Weak, owns us (transitively via ExtensionService).
  raw_ptr<Profile> profile_;
};

FontSettingsEventRouter::FontSettingsEventRouter(Profile* profile)
    : profile_(profile) {
  TRACE_EVENT0("browser,startup", "FontSettingsEventRouter::ctor");

  registrar_.Init(profile_->GetPrefs());

  // Unretained is safe here because the registrar is owned by this class.
  font_change_registrar_.Register(
      FontPrefChangeNotifierFactory::GetForProfile(profile),
      base::BindRepeating(&FontSettingsEventRouter::OnFontFamilyMapPrefChanged,
                          base::Unretained(this)));

  AddPrefToObserve(prefs::kWebKitDefaultFixedFontSize,
                   events::FONT_SETTINGS_ON_DEFAULT_FIXED_FONT_SIZE_CHANGED,
                   fonts::OnDefaultFixedFontSizeChanged::kEventName,
                   kPixelSizeKey);
  AddPrefToObserve(prefs::kWebKitDefaultFontSize,
                   events::FONT_SETTINGS_ON_DEFAULT_FONT_SIZE_CHANGED,
                   fonts::OnDefaultFontSizeChanged::kEventName, kPixelSizeKey);
  AddPrefToObserve(prefs::kWebKitMinimumFontSize,
                   events::FONT_SETTINGS_ON_MINIMUM_FONT_SIZE_CHANGED,
                   fonts::OnMinimumFontSizeChanged::kEventName, kPixelSizeKey);
}

FontSettingsEventRouter::~FontSettingsEventRouter() {}

void FontSettingsEventRouter::AddPrefToObserve(
    const char* pref_name,
    events::HistogramValue histogram_value,
    const char* event_name,
    const char* key) {
  registrar_.Add(pref_name,
                 base::BindRepeating(
                     &FontSettingsEventRouter::OnFontPrefChanged,
                     base::Unretained(this), histogram_value, event_name, key));
}

void FontSettingsEventRouter::OnFontFamilyMapPrefChanged(
    const std::string& pref_name) {
  std::string generic_family;
  std::string script;
  if (pref_names_util::ParseFontNamePrefPath(pref_name, &generic_family,
                                             &script)) {
    OnFontNamePrefChanged(pref_name, generic_family, script);
    return;
  }

  NOTREACHED_IN_MIGRATION();
}

void FontSettingsEventRouter::OnFontNamePrefChanged(
    const std::string& pref_name,
    const std::string& generic_family,
    const std::string& script) {
  const PrefService::Preference* pref = registrar_.prefs()->FindPreference(
      pref_name);
  CHECK(pref);

  if (!pref->GetValue()->is_string()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  std::string font_name = pref->GetValue()->GetString();
  base::Value::List args;
  base::Value::Dict dict;
  dict.Set(kFontIdKey, font_name);
  dict.Set(kGenericFamilyKey, generic_family);
  dict.Set(kScriptKey, script);
  args.Append(std::move(dict));

  extensions::preference_helpers::DispatchEventToExtensions(
      profile_, events::FONT_SETTINGS_ON_FONT_CHANGED,
      fonts::OnFontChanged::kEventName, std::move(args),
      extensions::mojom::APIPermissionID::kFontSettings, false, pref_name);
}

void FontSettingsEventRouter::OnFontPrefChanged(
    events::HistogramValue histogram_value,
    const std::string& event_name,
    const std::string& key,
    const std::string& pref_name) {
  const PrefService::Preference* pref = registrar_.prefs()->FindPreference(
      pref_name);
  CHECK(pref);

  base::Value::List args;
  base::Value::Dict dict;
  dict.Set(key, pref->GetValue()->Clone());
  args.Append(std::move(dict));

  extensions::preference_helpers::DispatchEventToExtensions(
      profile_, histogram_value, event_name, std::move(args),
      extensions::mojom::APIPermissionID::kFontSettings, false, pref_name);
}

FontSettingsAPI::FontSettingsAPI(content::BrowserContext* context)
    : font_settings_event_router_(
          new FontSettingsEventRouter(Profile::FromBrowserContext(context))) {}

FontSettingsAPI::~FontSettingsAPI() {
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<FontSettingsAPI>>::
    DestructorAtExit g_font_settings_api_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<FontSettingsAPI>*
FontSettingsAPI::GetFactoryInstance() {
  return g_font_settings_api_factory.Pointer();
}

ExtensionFunction::ResponseAction FontSettingsClearFontFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (profile->IsOffTheRecord())
    return RespondNow(Error(kSetFromIncognitoError));

  std::optional<fonts::ClearFont::Params> params =
      fonts::ClearFont::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string pref_path = GetFontNamePrefPath(params->details.generic_family,
                                              params->details.script);

  // Ensure `pref_path` really is for a registered per-script font pref.
  EXTENSION_FUNCTION_VALIDATE(profile->GetPrefs()->FindPreference(pref_path));

  ExtensionPrefsHelper::Get(profile)->RemoveExtensionControlledPref(
      extension_id(), pref_path, ChromeSettingScope::kRegular);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction FontSettingsGetFontFunction::Run() {
  std::optional<fonts::GetFont::Params> params =
      fonts::GetFont::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string pref_path = GetFontNamePrefPath(params->details.generic_family,
                                              params->details.script);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  PrefService* prefs = profile->GetPrefs();
  const PrefService::Preference* pref =
      prefs->FindPreference(pref_path);

  EXTENSION_FUNCTION_VALIDATE(pref && pref->GetValue()->is_string());
  std::string font_name = pref->GetValue()->GetString();

  // Legacy code was using the localized font name for fontId. These values may
  // have been stored in prefs. For backward compatibility, we are converting
  // the font name to the unlocalized name.
  MaybeUnlocalizeFontName(&font_name);

  // We don't support incognito-specific font prefs, so don't consider them when
  // getting level of control.
  const bool kIncognito = false;
  std::string level_of_control =
      extensions::preference_helpers::GetLevelOfControl(profile, extension_id(),
                                                        pref_path, kIncognito);

  base::Value::Dict result;
  result.Set(kFontIdKey, font_name);
  result.Set(kLevelOfControlKey, level_of_control);
  return RespondNow(WithArguments(std::move(result)));
}

ExtensionFunction::ResponseAction FontSettingsSetFontFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (profile->IsOffTheRecord())
    return RespondNow(Error(kSetFromIncognitoError));

  std::optional<fonts::SetFont::Params> params =
      fonts::SetFont::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string pref_path = GetFontNamePrefPath(params->details.generic_family,
                                              params->details.script);

  // Ensure `pref_path` really is for a registered font pref.
  EXTENSION_FUNCTION_VALIDATE(profile->GetPrefs()->FindPreference(pref_path));

  ExtensionPrefsHelper::Get(profile)->SetExtensionControlledPref(
      extension_id(), pref_path, ChromeSettingScope::kRegular,
      base::Value(params->details.font_id));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction FontSettingsGetFontListFunction::Run() {
  content::GetFontListAsync(
      BindOnce(&FontSettingsGetFontListFunction::FontListHasLoaded, this));
  return RespondLater();
}

void FontSettingsGetFontListFunction::FontListHasLoaded(
    base::Value::List list) {
  ExtensionFunction::ResponseValue response = CopyFontsToResult(list);
  Respond(std::move(response));
}

ExtensionFunction::ResponseValue
FontSettingsGetFontListFunction::CopyFontsToResult(
    const base::Value::List& fonts) {
  base::Value::List result;
  for (const auto& entry : fonts) {
    if (!entry.is_list()) {
      NOTREACHED_IN_MIGRATION();
      return Error("");
    }
    const base::Value::List& font_list_value = entry.GetList();

    if (font_list_value.size() < 2 || !font_list_value[0].is_string() ||
        !font_list_value[1].is_string()) {
      NOTREACHED_IN_MIGRATION();
      return Error("");
    }
    const std::string& name = font_list_value[0].GetString();
    const std::string& localized_name = font_list_value[1].GetString();

    base::Value::Dict font_name;
    font_name.Set(kFontIdKey, name);
    font_name.Set(kDisplayNameKey, localized_name);
    result.Append(std::move(font_name));
  }

  return WithArguments(std::move(result));
}

ExtensionFunction::ResponseAction ClearFontPrefExtensionFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (profile->IsOffTheRecord())
    return RespondNow(Error(kSetFromIncognitoError));

  ExtensionPrefsHelper::Get(profile)->RemoveExtensionControlledPref(
      extension_id(), GetPrefName(), ChromeSettingScope::kRegular);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction GetFontPrefExtensionFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  PrefService* prefs = profile->GetPrefs();
  const PrefService::Preference* pref = prefs->FindPreference(GetPrefName());
  EXTENSION_FUNCTION_VALIDATE(pref);

  // We don't support incognito-specific font prefs, so don't consider them when
  // getting level of control.
  const bool kIncognito = false;

  std::string level_of_control =
      extensions::preference_helpers::GetLevelOfControl(
          profile, extension_id(), GetPrefName(), kIncognito);

  base::Value::Dict result;
  result.Set(GetKey(), pref->GetValue()->Clone());
  result.Set(kLevelOfControlKey, level_of_control);
  return RespondNow(WithArguments(std::move(result)));
}

ExtensionFunction::ResponseAction SetFontPrefExtensionFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (profile->IsOffTheRecord())
    return RespondNow(Error(kSetFromIncognitoError));

  EXTENSION_FUNCTION_VALIDATE(args().size() >= 1);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_dict());
  const base::Value& details = args()[0];
  const base::Value* value = details.GetDict().Find(GetKey());
  EXTENSION_FUNCTION_VALIDATE(value);

  ExtensionPrefsHelper::Get(profile)->SetExtensionControlledPref(
      extension_id(), GetPrefName(), ChromeSettingScope::kRegular,
      value->Clone());
  return RespondNow(NoArguments());
}

const char* FontSettingsClearDefaultFontSizeFunction::GetPrefName() {
  return prefs::kWebKitDefaultFontSize;
}

const char* FontSettingsGetDefaultFontSizeFunction::GetPrefName() {
  return prefs::kWebKitDefaultFontSize;
}

const char* FontSettingsGetDefaultFontSizeFunction::GetKey() {
  return kPixelSizeKey;
}

const char* FontSettingsSetDefaultFontSizeFunction::GetPrefName() {
  return prefs::kWebKitDefaultFontSize;
}

const char* FontSettingsSetDefaultFontSizeFunction::GetKey() {
  return kPixelSizeKey;
}

const char* FontSettingsClearDefaultFixedFontSizeFunction::GetPrefName() {
  return prefs::kWebKitDefaultFixedFontSize;
}

const char* FontSettingsGetDefaultFixedFontSizeFunction::GetPrefName() {
  return prefs::kWebKitDefaultFixedFontSize;
}

const char* FontSettingsGetDefaultFixedFontSizeFunction::GetKey() {
  return kPixelSizeKey;
}

const char* FontSettingsSetDefaultFixedFontSizeFunction::GetPrefName() {
  return prefs::kWebKitDefaultFixedFontSize;
}

const char* FontSettingsSetDefaultFixedFontSizeFunction::GetKey() {
  return kPixelSizeKey;
}

const char* FontSettingsClearMinimumFontSizeFunction::GetPrefName() {
  return prefs::kWebKitMinimumFontSize;
}

const char* FontSettingsGetMinimumFontSizeFunction::GetPrefName() {
  return prefs::kWebKitMinimumFontSize;
}

const char* FontSettingsGetMinimumFontSizeFunction::GetKey() {
  return kPixelSizeKey;
}

const char* FontSettingsSetMinimumFontSizeFunction::GetPrefName() {
  return prefs::kWebKitMinimumFontSize;
}

const char* FontSettingsSetMinimumFontSizeFunction::GetKey() {
  return kPixelSizeKey;
}

}  // namespace extensions
