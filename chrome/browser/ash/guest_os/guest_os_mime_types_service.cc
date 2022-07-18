// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_mime_types_service.h"

#include <map>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/dbus/vm_applications/apps.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

using vm_tools::apps::App;

namespace guest_os {

namespace {

constexpr char kMimeTypeKey[] = "mime_type";

}  // namespace

GuestOsMimeTypesService::GuestOsMimeTypesService(Profile* profile)
    : prefs_(profile->GetPrefs()) {}

GuestOsMimeTypesService::~GuestOsMimeTypesService() = default;

// static
// TODO(crbug.com/1015353): Can be removed after M99.
void GuestOsMimeTypesService::MigrateVerboseMimeTypePrefs(
    PrefService* pref_service) {
  DictionaryPrefUpdate update(pref_service, prefs::kGuestOsMimeTypes);
  base::Value* mime_types = update.Get();
  std::map<std::string,
           std::map<std::string, std::map<std::string, std::string>>>
      migrated;
  std::vector<std::string> to_remove;

  for (const auto item : mime_types->DictItems()) {
    std::vector<std::string> parts = base::SplitString(
        item.first, "/", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (parts.size() == 1) {
      // Already migrated.
      continue;
    }

    // Migrate: "termina/penguin/txt": { "mime_type": "text/plain" } to:
    // "termina": { "penguin": { "txt": "text/plain" } }
    to_remove.push_back(item.first);
    std::string* mime_type;
    if (parts.size() == 3 && item.second.is_dict() &&
        (mime_type = item.second.FindStringKey(kMimeTypeKey))) {
      migrated[parts[0]][parts[1]][parts[2]] = *mime_type;
    } else {
      LOG(ERROR) << "Deleting unexpected crostini.mime_types key " << item.first
                 << "=" << item.second;
    }
  }

  // Delete old values.
  for (const std::string& s : to_remove) {
    mime_types->RemoveKey(s);
  }

  auto get_or_create = [](base::Value* v, const std::string& k) {
    base::Value* result = v->FindDictKey(k);
    if (!result) {
      result = v->SetKey(k, base::Value(base::Value::Type::DICTIONARY));
    }
    return result;
  };

  // Add migrated values.
  for (const auto& vm_item : migrated) {
    base::Value* vm = get_or_create(mime_types, vm_item.first);
    for (const auto& container_item : vm_item.second) {
      base::Value* container = get_or_create(vm, container_item.first);
      for (const auto& ext : container_item.second) {
        container->SetStringKey(ext.first, ext.second);
      }
    }
  }
}

std::string GuestOsMimeTypesService::GetMimeType(
    const base::FilePath& file_path,
    const std::string& vm_name,
    const std::string& container_name) const {
  const base::Value* vm =
      prefs_->GetDictionary(prefs::kGuestOsMimeTypes)->FindDictKey(vm_name);
  if (vm) {
    const base::Value* container = vm->FindDictKey(container_name);
    if (container) {
      // Try Extension() which may be a double like ".tar.gz".
      std::string extension = file_path.Extension();
      // Remove leading dot.
      extension.erase(0, 1);
      const std::string* result = container->FindStringKey(extension);
      if (!result) {
        // Try lowercase.
        result = container->FindStringKey(base::ToLowerASCII(extension));
      }
      // If this was a double extension, then try FinalExtension().
      if (!result && extension.find('.') != std::string::npos) {
        extension = file_path.FinalExtension();
        extension.erase(0, 1);
        result = container->FindStringKey(extension);
        if (!result) {
          // Try lowercase.
          result = container->FindStringKey(base::ToLowerASCII(extension));
        }
      }
      if (result) {
        return *result;
      }
    }
  }
  return "";
}

void GuestOsMimeTypesService::ClearMimeTypes(
    const std::string& vm_name,
    const std::string& container_name) {
  VLOG(1) << "ClearMimeTypes(" << vm_name << ", " << container_name << ")";
  DictionaryPrefUpdate update(prefs_, prefs::kGuestOsMimeTypes);
  base::Value* mime_types = update.Get();
  base::Value* vm = mime_types->FindDictKey(vm_name);
  if (vm) {
    vm->RemoveKey(container_name);
    if (container_name.empty() || vm->DictEmpty()) {
      mime_types->RemoveKey(vm_name);
    }
  }
}

void GuestOsMimeTypesService::UpdateMimeTypes(
    const vm_tools::apps::MimeTypes& mime_type_mappings) {
  if (mime_type_mappings.vm_name().empty()) {
    LOG(WARNING) << "Received MIME type list with missing VM name";
    return;
  }
  if (mime_type_mappings.container_name().empty()) {
    LOG(WARNING) << "Received MIME type list with missing container name";
    return;
  }

  base::Value exts(base::Value::Type::DICTIONARY);
  for (const auto& mapping : mime_type_mappings.mime_type_mappings()) {
    exts.SetStringKey(mapping.first, mapping.second);
  }
  VLOG(1) << "UpdateMimeTypes(" << mime_type_mappings.vm_name() << ", "
          << mime_type_mappings.container_name() << ")=" << exts;
  DictionaryPrefUpdate update(prefs_, prefs::kGuestOsMimeTypes);
  base::Value* mime_types = update.Get();
  base::Value* vm = mime_types->FindDictKey(mime_type_mappings.vm_name());
  if (!vm) {
    vm = mime_types->SetKey(mime_type_mappings.vm_name(),
                            base::Value(base::Value::Type::DICTIONARY));
  }
  vm->SetKey(mime_type_mappings.container_name(), std::move(exts));
}

}  // namespace guest_os
