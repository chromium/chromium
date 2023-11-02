// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_mime_types_service.h"

#include <map>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/dbus/vm_applications/apps.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "third_party/xdg_shared_mime_info/mime_cache.h"

namespace guest_os {

GuestOsMimeTypesService::GuestOsMimeTypesService(Profile* profile)
    : prefs_(profile->GetPrefs()) {}

GuestOsMimeTypesService::~GuestOsMimeTypesService() = default;

std::string GuestOsMimeTypesService::GetMimeType(
    const base::FilePath& file_path,
    const std::string& vm_name,
    const std::string& container_name) const {
  const base::Value::Dict* vm =
      prefs_->GetDict(prefs::kGuestOsMimeTypes).FindDict(vm_name);
  if (vm) {
    const base::Value::Dict* container = vm->FindDict(container_name);
    if (container) {
      // Try Extension() which may be a double like ".tar.gz".
      std::string extension = file_path.Extension();
      // Remove leading dot.
      extension.erase(0, 1);
      const std::string* result = container->FindString(extension);
      if (!result) {
        // Try lowercase.
        result = container->FindString(base::ToLowerASCII(extension));
      }
      // If this was a double extension, then try FinalExtension().
      if (!result && extension.find('.') != std::string::npos) {
        extension = file_path.FinalExtension();
        extension.erase(0, 1);
        result = container->FindString(extension);
        if (!result) {
          // Try lowercase.
          result = container->FindString(base::ToLowerASCII(extension));
        }
      }
      if (result) {
        return *result;
      }
    }
  }
  return "";
}

std::vector<std::string>
GuestOsMimeTypesService::GetExtensionTypesFromMimeTypes(
    const std::set<std::string>& supported_mime_types,
    const std::string& vm_name,
    const std::string& container_name) const {
  const base::Value::Dict* vm =
      prefs_->GetDict(prefs::kGuestOsMimeTypes).FindDict(vm_name);
  if (!vm) {
    return {};
  }
  const base::Value::Dict* container = vm->FindDict(container_name);
  if (!container) {
    return {};
  }
  const base::Value::Dict* extension_to_mime = vm->FindDict(container_name);
  if (!extension_to_mime) {
    return {};
  }

  std::vector<std::string> extension_types;
  for (auto entry : *extension_to_mime) {
    if (base::Contains(supported_mime_types, entry.second.GetString())) {
      extension_types.push_back(entry.first);
    }
  }
  return extension_types;
}

void GuestOsMimeTypesService::ClearMimeTypes(
    const std::string& vm_name,
    const std::string& container_name) {
  VLOG(1) << "ClearMimeTypes(" << vm_name << ", " << container_name << ")";
  ScopedDictPrefUpdate update(prefs_, prefs::kGuestOsMimeTypes);
  base::Value::Dict& mime_types = update.Get();
  base::Value::Dict* vm = mime_types.FindDict(vm_name);
  if (vm) {
    vm->Remove(container_name);
    if (container_name.empty() || vm->empty()) {
      mime_types.Remove(vm_name);
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
    // Only store mappings from container that are different to host.
    std::string type;
    if (!xdg_shared_mime_info::GetMimeCacheTypeFromExtension(mapping.first,
                                                             &type) ||
        mapping.second != type) {
      exts.SetStringKey(mapping.first, mapping.second);
    }
  }
  VLOG(1) << "UpdateMimeTypes(" << mime_type_mappings.vm_name() << ", "
          << mime_type_mappings.container_name() << ")=" << exts;
  ScopedDictPrefUpdate update(prefs_, prefs::kGuestOsMimeTypes);
  base::Value::Dict& mime_types = update.Get();
  base::Value::Dict* vm = mime_types.EnsureDict(mime_type_mappings.vm_name());
  vm->Set(mime_type_mappings.container_name(), std::move(exts));
}

}  // namespace guest_os
