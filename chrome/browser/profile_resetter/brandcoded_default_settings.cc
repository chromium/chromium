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
  if (!master_dictionary_)
    return false;
  const std::string* val =
      master_dictionary_->GetDict().FindString(prefs::kHomePage);
  if (!val)
    return false;
  *homepage = *val;
  return !homepage->empty();
}

absl::optional<bool> BrandcodedDefaultSettings::GetHomepageIsNewTab() const {
  return master_dictionary_
             ? master_dictionary_->FindBoolPath(prefs::kHomePageIsNewTabPage)
             : absl::nullopt;
}

absl::optional<bool> BrandcodedDefaultSettings::GetShowHomeButton() const {
  return master_dictionary_
             ? master_dictionary_->FindBoolPath(prefs::kShowHomeButton)
             : absl::nullopt;
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
  if (!master_dictionary_)
    return false;

  absl::optional<int> maybe_restore_on_startup =
      master_dictionary_->FindIntPath(prefs::kRestoreOnStartup);
  if (!maybe_restore_on_startup)
    return false;

  if (restore_on_startup)
    *restore_on_startup = *maybe_restore_on_startup;

  return true;
}

std::unique_ptr<base::ListValue>
BrandcodedDefaultSettings::GetUrlsToRestoreOnStartup() const {
  return ExtractList(prefs::kURLsToRestoreOnStartup);
}

std::unique_ptr<base::ListValue> BrandcodedDefaultSettings::ExtractList(
    const char* pref_name) const {
  const base::ListValue* value = nullptr;
  if (master_dictionary_ && master_dictionary_->GetList(pref_name, &value) &&
      !value->GetListDeprecated().empty()) {
    return base::ListValue::From(base::Value::ToUniquePtrValue(value->Clone()));
  }
  return nullptr;
}
