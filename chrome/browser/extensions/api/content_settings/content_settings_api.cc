// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/content_settings/content_settings_api.h"

#include <set>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/content_settings.h"
#include "components/content_settings/core/browser/content_settings_info.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_uma_util.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/webplugininfo.h"
#include "extensions/browser/api/content_settings/content_settings_helpers.h"
#include "extensions/browser/api/content_settings/content_settings_service.h"
#include "extensions/browser/api/content_settings/content_settings_store.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/api/extension_types.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "net/cookies/site_for_cookies.h"

using content::BrowserThread;

namespace Clear = extensions::api::content_settings::ContentSetting::Clear;
namespace Get = extensions::api::content_settings::ContentSetting::Get;
namespace Set = extensions::api::content_settings::ContentSetting::Set;

namespace {

using extensions::api::types::ChromeSettingScope;

bool RemoveContentType(base::Value::List& args,
                       ContentSettingsType* content_type) {
  if (args.empty() || !args[0].is_string())
    return false;

  // Not a ref since we remove the underlying value after.
  std::string content_type_str = args[0].GetString();

  // We remove the ContentSettingsType parameter since this is added by the
  // renderer, and is not part of the JSON schema.
  args.erase(args.begin());
  *content_type =
      extensions::content_settings_helpers::StringToContentSettingsType(
          content_type_str);
  return *content_type != ContentSettingsType::DEFAULT;
}

// Errors.
constexpr char kIncognitoContextError[] =
    "Can't modify regular settings from an incognito context.";
constexpr char kIncognitoSessionOnlyError[] =
    "You cannot read incognito content settings when no incognito window "
    "is open.";
constexpr char kInvalidUrlError[] = "The URL \"*\" is invalid.";

}  // namespace

namespace extensions {

ExtensionFunction::ResponseAction
ContentSettingsContentSettingClearFunction::Run() {
  ContentSettingsType content_type;
  EXTENSION_FUNCTION_VALIDATE(RemoveContentType(mutable_args(), &content_type));

  std::optional<Clear::Params> params = Clear::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (content_type == ContentSettingsType::DEPRECATED_PPAPI_BROKER) {
    NOTREACHED_IN_MIGRATION();
    return RespondNow(Error(kUnknownErrorDoNotUse));
  }

  ChromeSettingScope scope = ChromeSettingScope::kRegular;
  bool incognito = false;
  if (params->details.scope ==
      api::content_settings::Scope::kIncognitoSessionOnly) {
    scope = ChromeSettingScope::kIncognitoSessionOnly;
    incognito = true;
  }

  if (incognito) {
    // We don't check incognito permissions here, as an extension should be
    // always allowed to clear its own settings.
  } else if (browser_context()->IsOffTheRecord()) {
    // Incognito profiles can't access regular mode ever, they only exist in
    // split mode.
    return RespondNow(Error(kIncognitoContextError));
  }

  scoped_refptr<ContentSettingsStore> store =
      ContentSettingsService::Get(browser_context())->content_settings_store();
  store->ClearContentSettingsForExtensionAndContentType(extension_id(), scope,
                                                        content_type);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ContentSettingsContentSettingGetFunction::Run() {
  ContentSettingsType content_type;
  EXTENSION_FUNCTION_VALIDATE(RemoveContentType(mutable_args(), &content_type));

  std::optional<Get::Params> params = Get::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (content_type == ContentSettingsType::DEPRECATED_PPAPI_BROKER) {
    NOTREACHED_IN_MIGRATION();
    return RespondNow(Error(kUnknownErrorDoNotUse));
  }

  GURL primary_url(params->details.primary_url);
  if (!primary_url.is_valid()) {
    return RespondNow(Error(kInvalidUrlError, params->details.primary_url));
  }

  GURL secondary_url(primary_url);
  if (params->details.secondary_url) {
    secondary_url = GURL(*params->details.secondary_url);
    if (!secondary_url.is_valid()) {
      return RespondNow(
          Error(kInvalidUrlError, *params->details.secondary_url));
    }
  }

  bool incognito = false;
  if (params->details.incognito)
    incognito = *params->details.incognito;
  if (incognito && !include_incognito_information())
    return RespondNow(Error(extension_misc::kIncognitoErrorMessage));

  HostContentSettingsMap* map;
  scoped_refptr<content_settings::CookieSettings> cookie_settings;
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (incognito) {
    if (!profile->HasPrimaryOTRProfile()) {
      // TODO(bauerb): Allow reading incognito content settings
      // outside of an incognito session.
      return RespondNow(Error(kIncognitoSessionOnlyError));
    }
    map = HostContentSettingsMapFactory::GetForProfile(
        profile->GetPrimaryOTRProfile(/*create_if_needed=*/true));
    cookie_settings = CookieSettingsFactory::GetForProfile(
        profile->GetPrimaryOTRProfile(/*create_if_needed=*/true));
  } else {
    map = HostContentSettingsMapFactory::GetForProfile(profile);
    cookie_settings = CookieSettingsFactory::GetForProfile(profile);
  }

  // TODO(crbug.com/40247160): Consider whether the following check should
  // somehow determine real CookieSettingOverrides rather than default to none.
  net::SiteForCookies site_for_cookies =
      net::SiteForCookies::FromUrl(secondary_url);
  site_for_cookies.CompareWithFrameTreeSiteAndRevise(
      net::SchemefulSite(primary_url));
  ContentSetting setting =
      content_type == ContentSettingsType::COOKIES
          ? cookie_settings->GetCookieSetting(
                primary_url, site_for_cookies, secondary_url,
                net::CookieSettingOverrides(), nullptr)
          : map->GetContentSetting(primary_url, secondary_url, content_type);

  base::Value::Dict result;
  std::string setting_string =
      content_settings::ContentSettingToString(setting);
  DCHECK(!setting_string.empty());
  result.Set(ContentSettingsStore::kContentSettingKey, setting_string);

  return RespondNow(WithArguments(std::move(result)));
}

ExtensionFunction::ResponseAction
ContentSettingsContentSettingSetFunction::Run() {
  ContentSettingsType content_type;
  EXTENSION_FUNCTION_VALIDATE(RemoveContentType(mutable_args(), &content_type));

  std::optional<Set::Params> params = Set::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (content_type == ContentSettingsType::DEPRECATED_PPAPI_BROKER) {
    NOTREACHED_IN_MIGRATION();
    return RespondNow(Error(kUnknownErrorDoNotUse));
  }

  std::string primary_error;
  ContentSettingsPattern primary_pattern =
      content_settings_helpers::ParseExtensionPattern(
          params->details.primary_pattern, &primary_error);
  if (!primary_pattern.IsValid())
    return RespondNow(Error(primary_error));

  ContentSettingsPattern secondary_pattern = ContentSettingsPattern::Wildcard();
  if (params->details.secondary_pattern) {
    std::string secondary_error;
    secondary_pattern = content_settings_helpers::ParseExtensionPattern(
        *params->details.secondary_pattern, &secondary_error);
    if (!secondary_pattern.IsValid())
      return RespondNow(Error(secondary_error));
  }

  EXTENSION_FUNCTION_VALIDATE(params->details.setting.is_string());
  std::string setting_str = params->details.setting.GetString();
  ContentSetting setting;
  EXTENSION_FUNCTION_VALIDATE(
      content_settings::ContentSettingFromString(setting_str, &setting));
  // The content settings extensions API does not support setting any content
  // settings to |CONTENT_SETTING_DEFAULT|.
  EXTENSION_FUNCTION_VALIDATE(CONTENT_SETTING_DEFAULT != setting);
  EXTENSION_FUNCTION_VALIDATE(
      content_settings::ContentSettingsRegistry::GetInstance()
          ->Get(content_type)
          ->IsSettingValid(setting));

  const content_settings::ContentSettingsInfo* info =
      content_settings::ContentSettingsRegistry::GetInstance()->Get(
          content_type);

  // The ANTI_ABUSE content setting does not support site-specific settings.
  if (content_type == ContentSettingsType::ANTI_ABUSE &&
      (primary_pattern != ContentSettingsPattern::Wildcard() ||
       secondary_pattern != ContentSettingsPattern::Wildcard())) {
    return RespondNow(
        Error("Site-specific settings are not allowed for this type. The URL "
              "pattern must be '<all_urls>'."));
  }

  // Some content setting types support the full set of values listed in
  // content_settings.json only for exceptions. For the default setting,
  // some values might not be supported.
  // For example, camera supports [allow, ask, block] for exceptions, but only
  // [ask, block] for the default setting.
  if (primary_pattern == ContentSettingsPattern::Wildcard() &&
      secondary_pattern == ContentSettingsPattern::Wildcard() &&
      !info->IsDefaultSettingValid(setting)) {
    static const char kUnsupportedDefaultSettingError[] =
        "'%s' is not supported as the default setting of %s.";

    // TODO(msramek): Get the same human readable name as is presented
    // externally in the API, i.e. chrome.contentSettings.<name>.set().
    std::string readable_type_name;
    if (content_type == ContentSettingsType::MEDIASTREAM_MIC) {
      readable_type_name = "microphone";
    } else if (content_type == ContentSettingsType::MEDIASTREAM_CAMERA) {
      readable_type_name = "camera";
    } else {
      NOTREACHED_IN_MIGRATION()
          << "No human-readable type name defined for this type.";
    }

    return RespondNow(Error(base::StringPrintf(kUnsupportedDefaultSettingError,
                                               setting_str.c_str(),
                                               readable_type_name.c_str())));
  }

  if (primary_pattern != secondary_pattern &&
      secondary_pattern != ContentSettingsPattern::Wildcard()) {
    content_settings_uma_util::RecordContentSettingsHistogram(
        "ContentSettings.ExtensionEmbeddedSettingSet", content_type);
  } else {
    content_settings_uma_util::RecordContentSettingsHistogram(
        "ContentSettings.ExtensionNonEmbeddedSettingSet", content_type);
  }

  if (primary_pattern != secondary_pattern &&
      secondary_pattern != ContentSettingsPattern::Wildcard() &&
      !info->website_settings_info()->SupportsSecondaryPattern()) {
    static const char kUnsupportedEmbeddedException[] =
        "Embedded patterns are not supported for this setting.";
    return RespondNow(Error(kUnsupportedEmbeddedException));
  }

  ChromeSettingScope scope = ChromeSettingScope::kRegular;
  bool incognito = false;
  if (params->details.scope ==
      api::content_settings::Scope::kIncognitoSessionOnly) {
    scope = ChromeSettingScope::kIncognitoSessionOnly;
    incognito = true;
  }

  if (incognito) {
    // Regular profiles can't access incognito unless the extension is allowed
    // to run in incognito contexts.
    if (!browser_context()->IsOffTheRecord() &&
        !extensions::util::IsIncognitoEnabled(extension_id(),
                                              browser_context())) {
      return RespondNow(Error(extension_misc::kIncognitoErrorMessage));
    }
  } else {
    // Incognito profiles can't access regular mode ever, they only exist in
    // split mode.
    if (browser_context()->IsOffTheRecord())
      return RespondNow(Error(kIncognitoContextError));
  }

  if (scope == ChromeSettingScope::kIncognitoSessionOnly &&
      !Profile::FromBrowserContext(browser_context())->HasPrimaryOTRProfile()) {
    return RespondNow(Error(extension_misc::kIncognitoSessionOnlyErrorMessage));
  }

  scoped_refptr<ContentSettingsStore> store =
      ContentSettingsService::Get(browser_context())->content_settings_store();
  store->SetExtensionContentSetting(extension_id(), primary_pattern,
                                    secondary_pattern, content_type, setting,
                                    scope);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ContentSettingsContentSettingGetResourceIdentifiersFunction::Run() {
  // The only setting that supported resource identifiers was plugins. Since
  // plugins have been deprecated since Chrome 87, there are no resource
  // identifiers for existing settings (but we retain the function for
  // backwards and potential forwards compatibility).
  return RespondNow(NoArguments());
}

}  // namespace extensions
