// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_mime_types_service.h"

#include <map>
#include <string>
#include <vector>

#include "base/logging.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/vm_applications/apps.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

using vm_tools::apps::App;

namespace crostini {

namespace {

// Keys for the Dictionary stored in prefs for each MIME type mapping.
constexpr char kMimeTypeKey[] = "mime_type";
constexpr char kAppVmNameKey[] = "vm_name";
constexpr char kAppContainerNameKey[] = "container_name";

// This is for generating IDs which we use for storing the file extension keys
// unique to each VM/container.
std::string GenerateFileExtensionId(const std::string& file_extension,
                                    const std::string& vm_name,
                                    const std::string& container_name) {
  // These can collide in theory because the user could choose VM and container
  // names which contain slashes, but this will only result in MIME type
  // mappings not correlating properly.
  return vm_name + "/" + container_name + "/" + file_extension;
}

}  // namespace

CrostiniMimeTypesService::CrostiniMimeTypesService(Profile* profile)
    : prefs_(profile->GetPrefs()) {}

CrostiniMimeTypesService::~CrostiniMimeTypesService() = default;

std::string CrostiniMimeTypesService::GetMimeType(
    const base::FilePath& file_path,
    const std::string& vm_name,
    const std::string& container_name) const {
  std::string extension = file_path.FinalExtension();
  if (extension.empty()) {
    return "";
  }
  // Remove the leading dot character from the extension.
  extension.erase(0, 1);
  std::string extension_id =
      GenerateFileExtensionId(extension, vm_name, container_name);
  const base::DictionaryValue* mime_type_mappings =
      prefs_->GetDictionary(prefs::kCrostiniMimeTypes);
  const base::Value* extension_value = mime_type_mappings->FindKeyOfType(
      extension_id, base::Value::Type::DICTIONARY);
  if (!extension_value) {
    return "";
  }
  return extension_value->FindKeyOfType(kMimeTypeKey, base::Value::Type::STRING)
      ->GetString();
}

void CrostiniMimeTypesService::ClearMimeTypes(
    const std::string& vm_name,
    const std::string& container_name) {
  DictionaryPrefUpdate update(prefs_, prefs::kCrostiniMimeTypes);
  base::DictionaryValue* mime_type_mappings = update.Get();
  std::vector<std::string> removed_ids;
  for (const auto& item : mime_type_mappings->DictItems()) {
    if (item.second.FindKey(kAppVmNameKey)->GetString() == vm_name &&
        (container_name.empty() ||
         item.second.FindKey(kAppContainerNameKey)->GetString() ==
             container_name)) {
      removed_ids.push_back(item.first);
    }
  }
  for (const std::string& removed_id : removed_ids) {
    mime_type_mappings->RemoveKey(removed_id);
  }
}

void CrostiniMimeTypesService::UpdateMimeTypes(
    const vm_tools::apps::MimeTypes& mime_type_mappings) {
  if (mime_type_mappings.vm_name().empty()) {
    LOG(WARNING) << "Received MIME type list with missing VM name";
    return;
  }
  if (mime_type_mappings.container_name().empty()) {
    LOG(WARNING) << "Received MIME type list with missing container name";
    return;
  }

  // We need to compute the diff between the new mappings and the old mappings
  // (with matching vm/container names). We keep a set of the new extension ids
  // so that we can compute these and update the Dictionary directly.
  std::set<std::string> new_extension_ids;
  std::vector<std::string> removed_extensions;

  DictionaryPrefUpdate update(prefs_, prefs::kCrostiniMimeTypes);
  base::DictionaryValue* extensions = update.Get();
  for (const auto& mapping : mime_type_mappings.mime_type_mappings()) {
    std::string extension_id =
        GenerateFileExtensionId(mapping.first, mime_type_mappings.vm_name(),
                                mime_type_mappings.container_name());
    new_extension_ids.insert(extension_id);
    base::Value* old_extension = extensions->FindKey(extension_id);
    if (old_extension &&
        old_extension->FindKey(kMimeTypeKey)->GetString() == mapping.second) {
      // Old mapping matches the new one.
      continue;
    }

    base::Value pref_mapping(base::Value::Type::DICTIONARY);
    pref_mapping.SetKey(kMimeTypeKey, base::Value(mapping.second));
    pref_mapping.SetKey(kAppVmNameKey,
                        base::Value(mime_type_mappings.vm_name()));
    pref_mapping.SetKey(kAppContainerNameKey,
                        base::Value(mime_type_mappings.container_name()));

    extensions->SetKey(extension_id, std::move(pref_mapping));
  }

  for (const auto& item : extensions->DictItems()) {
    if (item.second.FindKey(kAppVmNameKey)->GetString() ==
            mime_type_mappings.vm_name() &&
        item.second.FindKey(kAppContainerNameKey)->GetString() ==
            mime_type_mappings.container_name() &&
        new_extension_ids.find(item.first) == new_extension_ids.end()) {
      removed_extensions.push_back(item.first);
    }
  }

  for (const std::string& removed_extension : removed_extensions) {
    extensions->RemoveKey(removed_extension);
  }
}

}  // namespace crostini
