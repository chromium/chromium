// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/brandcoded_default_settings.h"

#include <optional>

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
    std::unique_ptr<base::Value> root(json.Deserialize(nullptr, &error));
    if (!root.get()) {
      VLOG(1) << "Failed to parse brandcode prefs file: " << error;
      return;
    }
    if (!root->is_dict()) {
      VLOG(1) << "Failed to parse brandcode prefs file: "
              << "Root item must be a dictionary.";
      return;
    }
    master_dictionary_ = std::move(*root).TakeDict();
  }
}

BrandcodedDefaultSettings::~BrandcodedDefaultSettings() {
}

std::optional<base::Value::List>
BrandcodedDefaultSettings::GetSearchProviderOverrides() const {
  return ExtractList(prefs::kSearchProviderOverrides);
}

bool BrandcodedDefaultSettings::GetHomepage(std::string* homepage) const {
  const std::string* val = master_dictionary_.FindString(prefs::kHomePage);
  if (!val)
    return false;
  *homepage = *val;
  return !homepage->empty();
}

std::optional<bool> BrandcodedDefaultSettings::GetHomepageIsNewTab() const {
  return master_dictionary_.FindBoolByDottedPath(prefs::kHomePageIsNewTabPage);
}

std::optional<bool> BrandcodedDefaultSettings::GetShowHomeButton() const {
  return master_dictionary_.FindBoolByDottedPath(prefs::kShowHomeButton);
}

bool BrandcodedDefaultSettings::GetExtensions(
    std::vector<std::string>* extension_ids) const {
  DCHECK(extension_ids);
  const base::Value::Dict* extensions = master_dictionary_.FindDictByDottedPath(
      installer::initial_preferences::kExtensionsBlock);
  if (extensions) {
    for (const auto extension_id : *extensions) {
      if (crx_file::id_util::IdIsValid(extension_id.first))
        extension_ids->push_back(extension_id.first);
    }
    return true;
  }
  return false;
}

bool BrandcodedDefaultSettings::GetRestoreOnStartup(
    int* restore_on_startup) const {
  std::optional<int> maybe_restore_on_startup =
      master_dictionary_.FindIntByDottedPath(prefs::kRestoreOnStartup);
  if (!maybe_restore_on_startup)
    return false;

  if (restore_on_startup)
    *restore_on_startup = *maybe_restore_on_startup;

  return true;
}

std::optional<base::Value::List>
BrandcodedDefaultSettings::GetUrlsToRestoreOnStartup() const {
  return ExtractList(prefs::kURLsToRestoreOnStartup);
}

std::optional<base::Value::List> BrandcodedDefaultSettings::ExtractList(
    const char* pref_name) const {
  const base::Value::List* value =
      master_dictionary_.FindListByDottedPath(pref_name);
  if (value && !value->empty()) {
    return value->Clone();
  }
  return std::nullopt;
}
