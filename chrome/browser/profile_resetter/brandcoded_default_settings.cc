// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/brandcoded_default_settings.h"

#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/initial_preferences_constants.h"
#include "components/crx_file/id_util.h"
#include "components/search_engines/search_engines_pref_names.h"

BrandcodedDefaultSettings::BrandcodedDefaultSettings() {
}

BrandcodedDefaultSettings::BrandcodedDefaultSettings(const std::string& prefs) {
  if (!prefs.empty()) {
    JSONStringValueDeserializer json(prefs);
    std::string error;
    std::unique_ptr<base::Value> root(json.Deserialize(NULL, &error));
    if (!root.get()) {
      VLOG(1) << "Failed to parse brandcode prefs file: " << error;
      return;
    }
    if (!root->is_dict()) {
      VLOG(1) << "Failed to parse brandcode prefs file: "
              << "Root item must be a dictionary.";
      return;
    }
    master_dictionary_.reset(
        static_cast<base::DictionaryValue*>(root.release()));
  }
}

BrandcodedDefaultSettings::~BrandcodedDefaultSettings() {
}

std::unique_ptr<base::ListValue>
BrandcodedDefaultSettings::GetSearchProviderOverrides() const {
  return ExtractList(prefs::kSearchProviderOverrides);
}

bool BrandcodedDefaultSettings::GetHomepage(std::string* homepage) const {
  return master_dictionary_ &&
         master_dictionary_->GetString(prefs::kHomePage, homepage) &&
         !homepage->empty();
}

bool BrandcodedDefaultSettings::GetHomepageIsNewTab(
    bool* homepage_is_ntp) const {
  return master_dictionary_ &&
         master_dictionary_->GetBoolean(prefs::kHomePageIsNewTabPage,
                                        homepage_is_ntp);
}

bool BrandcodedDefaultSettings::GetShowHomeButton(
    bool* show_home_button) const {
  return master_dictionary_ &&
         master_dictionary_->GetBoolean(prefs::kShowHomeButton,
                                        show_home_button);
}

bool BrandcodedDefaultSettings::GetExtensions(
    std::vector<std::string>* extension_ids) const {
  DCHECK(extension_ids);
  base::DictionaryValue* extensions = NULL;
  if (master_dictionary_ &&
      master_dictionary_->GetDictionary(
          installer::initial_preferences::kExtensionsBlock, &extensions)) {
    for (base::DictionaryValue::Iterator extension_id(*extensions);
         !extension_id.IsAtEnd(); extension_id.Advance()) {
      if (crx_file::id_util::IdIsValid(extension_id.key()))
        extension_ids->push_back(extension_id.key());
    }
    return true;
  }
  return false;
}

bool BrandcodedDefaultSettings::GetRestoreOnStartup(
    int* restore_on_startup) const {
  return master_dictionary_ &&
         master_dictionary_->GetInteger(prefs::kRestoreOnStartup,
                                        restore_on_startup);
}

std::unique_ptr<base::ListValue>
BrandcodedDefaultSettings::GetUrlsToRestoreOnStartup() const {
  return ExtractList(prefs::kURLsToRestoreOnStartup);
}

std::unique_ptr<base::ListValue> BrandcodedDefaultSettings::ExtractList(
    const char* pref_name) const {
  const base::ListValue* value = NULL;
  if (master_dictionary_ &&
      master_dictionary_->GetList(pref_name, &value) &&
      !value->empty()) {
    return std::unique_ptr<base::ListValue>(value->DeepCopy());
  }
  return nullptr;
}
