// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/content_settings/content_settings_api.h"

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/api/content_settings/content_settings_api_constants.h"
#include "chrome/browser/extensions/api/content_settings/content_settings_helpers.h"
#include "chrome/browser/extensions/api/content_settings/content_settings_service.h"
#include "chrome/browser/extensions/api/content_settings/content_settings_store.h"
#include "chrome/browser/extensions/api/preference/preference_api_constants.h"
#include "chrome/browser/extensions/api/preference/preference_helpers.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/browser/plugins/plugin_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/content_settings.h"
#include "components/content_settings/core/browser/content_settings_info.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/webplugininfo.h"
#include "extensions/browser/extension_prefs_scope.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/error_utils.h"

using content::BrowserThread;
using content::PluginService;

namespace Clear = extensions::api::content_settings::ContentSetting::Clear;
namespace Get = extensions::api::content_settings::ContentSetting::Get;
namespace Set = extensions::api::content_settings::ContentSetting::Set;
namespace pref_helpers = extensions::preference_helpers;
namespace pref_keys = extensions::preference_api_constants;

namespace {

bool RemoveContentType(base::ListValue* args,
                       ContentSettingsType* content_type) {
  std::string content_type_str;
  if (!args->GetString(0, &content_type_str))
    return false;
  // We remove the ContentSettingsType parameter since this is added by the
  // renderer, and is not part of the JSON schema.
  args->Remove(0, nullptr);
  *content_type =
      extensions::content_settings_helpers::StringToContentSettingsType(
          content_type_str);
  return *content_type != ContentSettingsType::DEFAULT;
}

}  // namespace

namespace extensions {

ExtensionFunction::ResponseAction
ContentSettingsContentSettingClearFunction::Run() {
  ContentSettingsType content_type;
  EXTENSION_FUNCTION_VALIDATE(RemoveContentType(args_.get(), &content_type));

  std::unique_ptr<Clear::Params> params(Clear::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  ExtensionPrefsScope scope = kExtensionPrefsScopeRegular;
  bool incognito = false;
  if (params->details.scope ==
      api::content_settings::SCOPE_INCOGNITO_SESSION_ONLY) {
    scope = kExtensionPrefsScopeIncognitoSessionOnly;
    incognito = true;
  }

  if (incognito) {
    // We don't check incognito permissions here, as an extension should be
    // always allowed to clear its own settings.
  } else if (browser_context()->IsOffTheRecord()) {
    // Incognito profiles can't access regular mode ever, they only exist in
    // split mode.
    return RespondNow(
        Error(content_settings_api_constants::kIncognitoContextError));
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
  EXTENSION_FUNCTION_VALIDATE(RemoveContentType(args_.get(), &content_type));

  std::unique_ptr<Get::Params> params(Get::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  GURL primary_url(params->details.primary_url);
  if (!primary_url.is_valid()) {
    return RespondNow(Error(content_settings_api_constants::kInvalidUrlError,
                            params->details.primary_url));
  }

  GURL secondary_url(primary_url);
  if (params->details.secondary_url.get()) {
    secondary_url = GURL(*params->details.secondary_url);
    if (!secondary_url.is_valid()) {
      return RespondNow(Error(content_settings_api_constants::kInvalidUrlError,
                              *params->details.secondary_url));
    }
  }

  std::string resource_identifier;
  if (params->details.resource_identifier.get())
    resource_identifier = params->details.resource_identifier->id;

  bool incognito = false;
  if (params->details.incognito.get())
    incognito = *params->details.incognito;
  if (incognito && !include_incognito_information())
    return RespondNow(Error(pref_keys::kIncognitoErrorMessage));

  HostContentSettingsMap* map;
  content_settings::CookieSettings* cookie_settings;
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (incognito) {
    if (!profile->HasOffTheRecordProfile()) {
      // TODO(bauerb): Allow reading incognito content settings
      // outside of an incognito session.
      return RespondNow(
          Error(content_settings_api_constants::kIncognitoSessionOnlyError));
    }
    map = HostContentSettingsMapFactory::GetForProfile(
        profile->GetOffTheRecordProfile());
    cookie_settings =
        CookieSettingsFactory::GetForProfile(profile->GetOffTheRecordProfile())
            .get();
  } else {
    map = HostContentSettingsMapFactory::GetForProfile(profile);
    cookie_settings = CookieSettingsFactory::GetForProfile(profile).get();
  }

  ContentSetting setting;
  if (content_type == ContentSettingsType::COOKIES) {
    cookie_settings->GetCookieSetting(primary_url, secondary_url, nullptr,
                                      &setting);
  } else {
    setting = map->GetContentSetting(primary_url, secondary_url, content_type,
                                     resource_identifier);
  }

  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());
  std::string setting_string =
      content_settings::ContentSettingToString(setting);
  DCHECK(!setting_string.empty());
  result->SetString(content_settings_api_constants::kContentSettingKey,
                    setting_string);

  return RespondNow(OneArgument(std::move(result)));
}

ExtensionFunction::ResponseAction
ContentSettingsContentSettingSetFunction::Run() {
  ContentSettingsType content_type;
  EXTENSION_FUNCTION_VALIDATE(RemoveContentType(args_.get(), &content_type));

  std::unique_ptr<Set::Params> params(Set::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string primary_error;
  ContentSettingsPattern primary_pattern =
      content_settings_helpers::ParseExtensionPattern(
          params->details.primary_pattern, &primary_error);
  if (!primary_pattern.IsValid())
    return RespondNow(Error(primary_error));

  ContentSettingsPattern secondary_pattern = ContentSettingsPattern::Wildcard();
  if (params->details.secondary_pattern.get()) {
    std::string secondary_error;
    secondary_pattern = content_settings_helpers::ParseExtensionPattern(
        *params->details.secondary_pattern, &secondary_error);
    if (!secondary_pattern.IsValid())
      return RespondNow(Error(secondary_error));
  }

  std::string resource_identifier;
  if (params->details.resource_identifier.get())
    resource_identifier = params->details.resource_identifier->id;

  std::string setting_str;
  EXTENSION_FUNCTION_VALIDATE(
      params->details.setting->GetAsString(&setting_str));
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
      NOTREACHED() << "No human-readable type name defined for this type.";
    }

    return RespondNow(Error(base::StringPrintf(kUnsupportedDefaultSettingError,
                                               setting_str.c_str(),
                                               readable_type_name.c_str())));
  }

  size_t num_values = 0;
  int histogram_value =
      ContentSettingTypeToHistogramValue(content_type, &num_values);
  if (primary_pattern != secondary_pattern &&
      secondary_pattern != ContentSettingsPattern::Wildcard()) {
    UMA_HISTOGRAM_EXACT_LINEAR("ContentSettings.ExtensionEmbeddedSettingSet",
                               histogram_value, num_values);
  } else {
    UMA_HISTOGRAM_EXACT_LINEAR("ContentSettings.ExtensionNonEmbeddedSettingSet",
                               histogram_value, num_values);
  }

  if (primary_pattern != secondary_pattern &&
      secondary_pattern != ContentSettingsPattern::Wildcard() &&
      !info->website_settings_info()->SupportsEmbeddedExceptions() &&
      base::FeatureList::IsEnabled(::features::kPermissionDelegation)) {
    static const char kUnsupportedEmbeddedException[] =
        "Embedded patterns are not supported for this setting.";
    return RespondNow(Error(kUnsupportedEmbeddedException));
  }

  ExtensionPrefsScope scope = kExtensionPrefsScopeRegular;
  bool incognito = false;
  if (params->details.scope ==
      api::content_settings::SCOPE_INCOGNITO_SESSION_ONLY) {
    scope = kExtensionPrefsScopeIncognitoSessionOnly;
    incognito = true;
  }

  if (incognito) {
    // Regular profiles can't access incognito unless the extension is allowed
    // to run in incognito contexts.
    if (!browser_context()->IsOffTheRecord() &&
        !extensions::util::IsIncognitoEnabled(extension_id(),
                                              browser_context())) {
      return RespondNow(Error(pref_keys::kIncognitoErrorMessage));
    }
  } else {
    // Incognito profiles can't access regular mode ever, they only exist in
    // split mode.
    if (browser_context()->IsOffTheRecord())
      return RespondNow(
          Error(content_settings_api_constants::kIncognitoContextError));
  }

  if (scope == kExtensionPrefsScopeIncognitoSessionOnly &&
      !Profile::FromBrowserContext(browser_context())
           ->HasOffTheRecordProfile()) {
    return RespondNow(Error(pref_keys::kIncognitoSessionOnlyErrorMessage));
  }

  scoped_refptr<ContentSettingsStore> store =
      ContentSettingsService::Get(browser_context())->content_settings_store();
  store->SetExtensionContentSetting(extension_id(), primary_pattern,
                                    secondary_pattern, content_type,
                                    resource_identifier, setting, scope);
  return RespondNow(NoArguments());
}

bool ContentSettingsContentSettingGetResourceIdentifiersFunction::RunAsync() {
  ContentSettingsType content_type;
  EXTENSION_FUNCTION_VALIDATE(RemoveContentType(args_.get(), &content_type));

  if (content_type != ContentSettingsType::PLUGINS) {
    SendResponse(true);
    return true;
  }

  PluginService::GetInstance()->GetPlugins(base::BindOnce(
      &ContentSettingsContentSettingGetResourceIdentifiersFunction::
          OnGotPlugins,
      this));
  return true;
}

void ContentSettingsContentSettingGetResourceIdentifiersFunction::OnGotPlugins(
    const std::vector<content::WebPluginInfo>& plugins) {
  PluginFinder* finder = PluginFinder::GetInstance();
  std::set<std::string> group_identifiers;
  std::unique_ptr<base::ListValue> list(new base::ListValue());
  for (auto it = plugins.cbegin(); it != plugins.cend(); ++it) {
    std::unique_ptr<PluginMetadata> plugin_metadata(
        finder->GetPluginMetadata(*it));
    const std::string& group_identifier = plugin_metadata->identifier();
    if (group_identifiers.find(group_identifier) != group_identifiers.end())
      continue;

    group_identifiers.insert(group_identifier);
    std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
    dict->SetString(content_settings_api_constants::kIdKey, group_identifier);
    dict->SetString(content_settings_api_constants::kDescriptionKey,
                    plugin_metadata->name());
    list->Append(std::move(dict));
  }
  SetResult(std::move(list));
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &ContentSettingsContentSettingGetResourceIdentifiersFunction::
              SendResponse,
          this, true));
}

}  // namespace extensions
