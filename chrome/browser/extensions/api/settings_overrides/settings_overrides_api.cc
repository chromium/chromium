// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/settings_overrides/settings_overrides_api.h"

#include <stddef.h>
#include <memory>
#include <utility>

#include "base/lazy_instance.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/preference/preference_api.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/extensions/manifest_handlers/settings_overrides_handler.h"
#include "chrome/common/pref_names.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data_util.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace {

base::LazyInstance<BrowserContextKeyedAPIFactory<SettingsOverridesAPI>>::
    DestructorAtExit g_settings_overrides_api_factory =
        LAZY_INSTANCE_INITIALIZER;

const char kManyStartupPagesWarning[] = "* specifies more than 1 startup URL. "
    "All but the first will be ignored.";

using api::manifest_types::ChromeSettingsOverrides;

std::string SubstituteInstallParam(std::string str,
                                   const std::string& install_parameter) {
  base::ReplaceSubstringsAfterOffset(&str, 0, "__PARAM__", install_parameter);
  return str;
}

std::unique_ptr<TemplateURLData> ConvertSearchProvider(
    PrefService* prefs,
    const ChromeSettingsOverrides::Search_provider& search_provider,
    const std::string& install_parameter) {
  std::unique_ptr<TemplateURLData> data;
  if (search_provider.prepopulated_id) {
    data = TemplateURLPrepopulateData::GetPrepopulatedEngine(
        prefs, *search_provider.prepopulated_id);
    if (!data) {
      VLOG(1) << "Settings Overrides API can't recognize prepopulated_id="
          << *search_provider.prepopulated_id;
    }
  }

  if (!data)
    data = std::make_unique<TemplateURLData>();

  if (search_provider.name)
    data->SetShortName(base::UTF8ToUTF16(*search_provider.name));
  if (search_provider.keyword)
    data->SetKeyword(base::UTF8ToUTF16(*search_provider.keyword));
  data->SetURL(
      SubstituteInstallParam(search_provider.search_url, install_parameter));
  if (search_provider.suggest_url) {
    data->suggestions_url =
        SubstituteInstallParam(*search_provider.suggest_url, install_parameter);
  }
  if (search_provider.image_url) {
    data->image_url =
        SubstituteInstallParam(*search_provider.image_url, install_parameter);
  }
  if (search_provider.search_url_post_params)
    data->search_url_post_params = *search_provider.search_url_post_params;
  if (search_provider.suggest_url_post_params)
    data->suggestions_url_post_params =
        *search_provider.suggest_url_post_params;
  if (search_provider.image_url_post_params)
    data->image_url_post_params = *search_provider.image_url_post_params;
  if (search_provider.favicon_url) {
    data->favicon_url = GURL(SubstituteInstallParam(
        *search_provider.favicon_url, install_parameter));
  }
  data->safe_for_autoreplace = false;
  if (search_provider.encoding) {
    data->input_encodings.clear();
    data->input_encodings.push_back(*search_provider.encoding);
  }
  data->date_created = base::Time();
  data->last_modified = base::Time();
  data->prepopulate_id = 0;
  if (search_provider.alternate_urls) {
    data->alternate_urls.clear();
    for (size_t i = 0; i < search_provider.alternate_urls->size(); ++i) {
      if (!search_provider.alternate_urls->at(i).empty())
        data->alternate_urls.push_back(SubstituteInstallParam(
            search_provider.alternate_urls->at(i), install_parameter));
    }
  }
  return data;
}

}  // namespace

SettingsOverridesAPI::SettingsOverridesAPI(content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)),
      url_service_(TemplateURLServiceFactory::GetForProfile(profile_)) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(profile_));
}

SettingsOverridesAPI::~SettingsOverridesAPI() {
}

BrowserContextKeyedAPIFactory<SettingsOverridesAPI>*
SettingsOverridesAPI::GetFactoryInstance() {
  return g_settings_overrides_api_factory.Pointer();
}

void SettingsOverridesAPI::SetPref(const std::string& extension_id,
                                   const std::string& pref_key,
                                   std::unique_ptr<base::Value> value) const {
  PreferenceAPI* prefs = PreferenceAPI::Get(profile_);
  if (!prefs)
    return;  // Expected in unit tests.
  DCHECK(value);
  prefs->SetExtensionControlledPref(
      extension_id, pref_key, kExtensionPrefsScopeRegular,
      base::Value::FromUniquePtrValue(std::move(value)));
}

void SettingsOverridesAPI::UnsetPref(const std::string& extension_id,
                                     const std::string& pref_key) const {
  PreferenceAPI* prefs = PreferenceAPI::Get(profile_);
  if (!prefs)
    return;  // Expected in unit tests.
  prefs->RemoveExtensionControlledPref(
      extension_id,
      pref_key,
      kExtensionPrefsScopeRegular);
}

void SettingsOverridesAPI::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  const SettingsOverrides* settings = SettingsOverrides::Get(extension);
  if (settings) {
    std::string install_parameter =
        ExtensionPrefs::Get(profile_)->GetInstallParam(extension->id());
    if (settings->homepage) {
      SetPref(extension->id(), prefs::kHomePage,
              std::make_unique<base::Value>(SubstituteInstallParam(
                  settings->homepage->spec(), install_parameter)));
      SetPref(extension->id(), prefs::kHomePageIsNewTabPage,
              std::make_unique<base::Value>(false));
    }
    if (!settings->startup_pages.empty()) {
      SetPref(
          extension->id(), prefs::kRestoreOnStartup,
          std::make_unique<base::Value>(SessionStartupPref::kPrefValueURLs));
      if (settings->startup_pages.size() > 1) {
        VLOG(1) << extensions::ErrorUtils::FormatErrorMessage(
                       kManyStartupPagesWarning,
                       manifest_keys::kSettingsOverride);
      }
      std::unique_ptr<base::ListValue> url_list(new base::ListValue);
      url_list->AppendString(SubstituteInstallParam(
          settings->startup_pages[0].spec(), install_parameter));
      SetPref(extension->id(), prefs::kURLsToRestoreOnStartup,
              std::move(url_list));
    }
    if (settings->search_engine) {
      // Bring the preference to the correct state. Before this code set it
      // to "true" for all search engines. Thus, we should overwrite it for
      // all search engines.
      if (settings->search_engine->is_default) {
        SetPref(extension->id(), prefs::kDefaultSearchProviderEnabled,
                std::make_unique<base::Value>(true));
      } else {
        UnsetPref(extension->id(), prefs::kDefaultSearchProviderEnabled);
      }
      DCHECK(url_service_);
      RegisterSearchProvider(extension);
    }
  }
}
void SettingsOverridesAPI::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  const SettingsOverrides* settings = SettingsOverrides::Get(extension);
  if (settings) {
    if (settings->homepage) {
      UnsetPref(extension->id(), prefs::kHomePage);
      UnsetPref(extension->id(), prefs::kHomePageIsNewTabPage);
    }
    if (!settings->startup_pages.empty()) {
      UnsetPref(extension->id(), prefs::kRestoreOnStartup);
      UnsetPref(extension->id(), prefs::kURLsToRestoreOnStartup);
    }
    if (settings->search_engine) {
      if (settings->search_engine->is_default) {
        // Current extension can be overriding DSE.
        UnsetPref(extension->id(),
                  DefaultSearchManager::kDefaultSearchProviderDataPrefName);
      }
      DCHECK(url_service_);
      url_service_->RemoveExtensionControlledTURL(
          extension->id(), TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION);
    }
  }
}

void SettingsOverridesAPI::RegisterSearchProvider(
    const Extension* extension) const {
  DCHECK(url_service_);
  DCHECK(extension);
  const SettingsOverrides* settings = SettingsOverrides::Get(extension);
  DCHECK(settings);
  DCHECK(settings->search_engine);

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile_);
  std::string install_parameter = prefs->GetInstallParam(extension->id());
  std::unique_ptr<TemplateURLData> data = ConvertSearchProvider(
      profile_->GetPrefs(), *settings->search_engine, install_parameter);
  auto turl = std::make_unique<TemplateURL>(
      *data, TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION, extension->id(),
      prefs->GetInstallTime(extension->id()),
      settings->search_engine->is_default);

  url_service_->Add(std::move(turl));

  if (settings->search_engine->is_default) {
    // Override current DSE pref to have extension overriden value.
    SetPref(extension->id(),
            DefaultSearchManager::kDefaultSearchProviderDataPrefName,
            TemplateURLDataToDictionary(*data));
  }
}

template <>
void BrowserContextKeyedAPIFactory<
    SettingsOverridesAPI>::DeclareFactoryDependencies() {
  DependsOn(ExtensionPrefsFactory::GetInstance());
  DependsOn(PreferenceAPI::GetFactoryInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

}  // namespace extensions
