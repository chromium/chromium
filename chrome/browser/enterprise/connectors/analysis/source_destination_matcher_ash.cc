// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/source_destination_matcher_ash.h"

#include <optional>

#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/values.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "storage/browser/file_system/file_system_url.h"

namespace storage {
class FileSystemURL;
}

namespace enterprise_connectors {

namespace {
// Checks if the key of the sources or destinations list is known.
// This should only be extended when a key is properly supported.
bool AllowedFSInfoKey(const std::string& key) {
  // TODO(crbug.com/1340553): Also allow settings for app ids and smb.
  return key == "file_system_type";
}

// This function checks whether there are no unknown keys in the passed dict.
// The function returns true if there are no such unknown keys and false if an
// unknown key is found.
bool SourceOrDestinationEntryIsValid(const base::Value::Dict* dict) {
  DCHECK(dict);
  for (auto&& [key, _] : *dict) {
    if (!AllowedFSInfoKey(key)) {
      LOG(ERROR) << "Source or destination entry is not valid, ignoring it "
                    "because of unknown key \""
                 << key << "\" in " << dict->DebugString();
      return false;
    }
  }
  return true;
}
}  // namespace

// static
SourceDestinationMatcherAsh::FsType SourceDestinationMatcherAsh::VolumeToFsType(
    base::WeakPtr<file_manager::Volume> volume) {
  if (!volume) {
    return SourceDestinationMatcherAsh::FsType::kUnknown;
  }
  if (volume->type() == file_manager::VOLUME_TYPE_GUEST_OS) {
    if (!volume->vm_type()) {
      return SourceDestinationMatcherAsh::FsType::kUnknownVm;
    }
    switch (volume->vm_type().value()) {
      case guest_os::VmType::TERMINA:
        return SourceDestinationMatcherAsh::FsType::kCrostini;
      case guest_os::VmType::PLUGIN_VM:
        return SourceDestinationMatcherAsh::FsType::kPluginVm;
      case guest_os::VmType::BOREALIS:
        return SourceDestinationMatcherAsh::FsType::kBorealis;
      case guest_os::VmType::BRUSCHETTA:
        return SourceDestinationMatcherAsh::FsType::kBruschetta;
      case guest_os::VmType::UNKNOWN:
        return SourceDestinationMatcherAsh::FsType::kUnknownVm;
      case guest_os::VmType::ARCVM:
        return SourceDestinationMatcherAsh::FsType::kArc;
      case guest_os::VmType::BAGUETTE:
        return SourceDestinationMatcherAsh::FsType::kUnknownVm;
      case guest_os::VmType::VmType_INT_MIN_SENTINEL_DO_NOT_USE_:
      case guest_os::VmType::VmType_INT_MAX_SENTINEL_DO_NOT_USE_:
        NOTREACHED_IN_MIGRATION();
    }
  }
  switch (volume->type()) {
    case file_manager::VOLUME_TYPE_TESTING:
      return SourceDestinationMatcherAsh::FsType::kTesting;
    case file_manager::VOLUME_TYPE_GOOGLE_DRIVE:
      return SourceDestinationMatcherAsh::FsType::kGoogleDrive;
    case file_manager::VOLUME_TYPE_DOWNLOADS_DIRECTORY:
      return SourceDestinationMatcherAsh::FsType::kMyFiles;
    case file_manager::VOLUME_TYPE_REMOVABLE_DISK_PARTITION:
      return SourceDestinationMatcherAsh::FsType::kRemovable;
    case file_manager::VOLUME_TYPE_PROVIDED:
      return SourceDestinationMatcherAsh::FsType::kProvided;
    case file_manager::VOLUME_TYPE_MTP:
      return SourceDestinationMatcherAsh::FsType::kDeviceMediaStorage;
    case file_manager::VOLUME_TYPE_MEDIA_VIEW:
      // The media view file system is a read-only file system that allows to
      // display recent ARC files in the Files App.
      return SourceDestinationMatcherAsh::FsType::kArc;
    case file_manager::VOLUME_TYPE_CROSTINI:
      return SourceDestinationMatcherAsh::FsType::kCrostini;
    case file_manager::VOLUME_TYPE_ANDROID_FILES:
      return SourceDestinationMatcherAsh::FsType::kArc;
    case file_manager::VOLUME_TYPE_DOCUMENTS_PROVIDER:
      // This is for ARC documents provider, so we also map to ARC!
      return SourceDestinationMatcherAsh::FsType::kArc;
    case file_manager::VOLUME_TYPE_SMB:
      return SourceDestinationMatcherAsh::FsType::kSmb;
    case file_manager::VOLUME_TYPE_SYSTEM_INTERNAL:
      return SourceDestinationMatcherAsh::FsType::kUnknown;
    case file_manager::VOLUME_TYPE_MOUNTED_ARCHIVE_FILE:
    case file_manager::VOLUME_TYPE_GUEST_OS:
    case file_manager::NUM_VOLUME_TYPE:
      NOTREACHED_IN_MIGRATION();
  }
  NOTREACHED_IN_MIGRATION();
  return SourceDestinationMatcherAsh::FsType::kUnknown;
}

// static
SourceDestinationMatcherAsh::FsType SourceDestinationMatcherAsh::PathToFsType(
    content::BrowserContext* context,
    const base::FilePath& path) {
  file_manager::VolumeManager* const volume_manager =
      file_manager::VolumeManager::Get(context);

  DCHECK(volume_manager);
  base::WeakPtr<file_manager::Volume> volume =
      volume_manager->FindVolumeFromPath(path);

  if (volume &&
      volume->type() == file_manager::VOLUME_TYPE_MOUNTED_ARCHIVE_FILE) {
    // For mounted archives, we check the source file system.
    // Recursive to handle mounted archives from mounted archives.
    return PathToFsType(context, volume->source_path());
  }

  return VolumeToFsType(volume);
}

std::string SourceDestinationMatcherAsh::GetVolumeDescriptionFromPath(
    content::BrowserContext* context,
    const base::FilePath& path) {
  return FsTypeToString(PathToFsType(context, path));
}

// static
std::optional<SourceDestinationMatcherAsh::FsType>
SourceDestinationMatcherAsh::StringToFsType(const std::string& s) {
  if (s == "TESTING") {
    return SourceDestinationMatcherAsh::FsType::kTesting;
  }
  if (s == "UNKNOWN") {
    return SourceDestinationMatcherAsh::FsType::kUnknown;
  }
  if (s == "*" || s == "ANY") {
    return SourceDestinationMatcherAsh::FsType::kAny;
  }
  if (s == "MY_FILES") {
    return SourceDestinationMatcherAsh::FsType::kMyFiles;
  }
  if (s == "REMOVABLE") {
    return SourceDestinationMatcherAsh::FsType::kRemovable;
  }
  if (s == "DEVICE_MEDIA_STORAGE") {
    return SourceDestinationMatcherAsh::FsType::kDeviceMediaStorage;
  }
  if (s == "PROVIDED") {
    return SourceDestinationMatcherAsh::FsType::kProvided;
  }
  if (s == "ARC") {
    return SourceDestinationMatcherAsh::FsType::kArc;
  }
  if (s == "GOOGLE_DRIVE") {
    return SourceDestinationMatcherAsh::FsType::kGoogleDrive;
  }
  if (s == "SMB") {
    return SourceDestinationMatcherAsh::FsType::kSmb;
  }
  if (s == "CROSTINI") {
    return SourceDestinationMatcherAsh::FsType::kCrostini;
  }
  if (s == "PLUGIN_VM") {
    return SourceDestinationMatcherAsh::FsType::kPluginVm;
  }
  if (s == "BOREALIS") {
    return SourceDestinationMatcherAsh::FsType::kBorealis;
  }
  if (s == "BRUSCHETTA") {
    return SourceDestinationMatcherAsh::FsType::kBruschetta;
  }
  if (s == "UNKNOWN_VM") {
    return SourceDestinationMatcherAsh::FsType::kUnknownVm;
  }
  return std::nullopt;
}

std::set<SourceDestinationMatcherAsh::FsType>
SourceDestinationMatcherAsh::ValueListToFsTypes(
    const base::Value::List* source_or_destination_list) {
  std::set<SourceDestinationMatcherAsh::FsType> fs_types;
  for (const auto& entry : *source_or_destination_list) {
    const auto* dict = entry.GetIfDict();
    if (!dict) {
      LOG(ERROR) << "Entry is not a dict.";
      continue;
    }
    if (!SourceOrDestinationEntryIsValid(dict)) {
      // We ignore all entries that have unknown configuration values.
      // These values typically narrow down the range of matching file system
      // urls and thus are handled as a specialized case.
      // These specialized cases are ignored, if they are not yet supported.
      continue;
    }
    const auto* s = dict->FindString("file_system_type");
    if (!s || s->empty()) {
      LOG(ERROR) << "file_system_type not found.";
      continue;
    }
    auto fs_type = StringToFsType(*s);
    if (fs_type) {
      fs_types.insert(fs_type.value());
    }
  }
  return fs_types;
}

std::string SourceDestinationMatcherAsh::FsTypeToString(
    SourceDestinationMatcherAsh::FsType fs_type) {
  switch (fs_type) {
    case SourceDestinationMatcherAsh::FsType::kTesting:
      return "TESTING";
    case SourceDestinationMatcherAsh::FsType::kUnknown:
      return "UNKNOWN";
    case SourceDestinationMatcherAsh::FsType::kAny:
      return "ANY";
    case SourceDestinationMatcherAsh::FsType::kMyFiles:
      return "MY_FILES";
    case SourceDestinationMatcherAsh::FsType::kRemovable:
      return "REMOVABLE";
    case SourceDestinationMatcherAsh::FsType::kDeviceMediaStorage:
      return "DEVICE_MEDIA_STORAGE";
    case SourceDestinationMatcherAsh::FsType::kProvided:
      return "PROVIDED";
    case SourceDestinationMatcherAsh::FsType::kArc:
      return "ARC";
    case SourceDestinationMatcherAsh::FsType::kGoogleDrive:
      return "GOOGLE_DRIVE";
    case SourceDestinationMatcherAsh::FsType::kSmb:
      return "SMB";
    case SourceDestinationMatcherAsh::FsType::kCrostini:
      return "CROSTINI";
    case SourceDestinationMatcherAsh::FsType::kPluginVm:
      return "PLUGIN_VM";
    case SourceDestinationMatcherAsh::FsType::kBorealis:
      return "BOREALIS";
    case SourceDestinationMatcherAsh::FsType::kBruschetta:
      return "BRUSCHETTA";
    case SourceDestinationMatcherAsh::FsType::kUnknownVm:
      return "UNKNOWN_VM";
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

SourceDestinationMatcherAsh::SourceDestinationMatcherAsh() = default;

SourceDestinationMatcherAsh::~SourceDestinationMatcherAsh() = default;

void SourceDestinationMatcherAsh::AddFilters(
    ID* id,
    const base::Value::List* settings_list) {
  DCHECK(id);
  // TODO(crbug.com/1340553): Adapt for app ids and smb settings
  if (!settings_list) {
    LOG(ERROR) << "No settings list found.";
    return;
  }
  for (const auto& value : *settings_list) {
    const auto* dict = value.GetIfDict();
    if (!dict) {
      LOG(ERROR) << "Settings list value is not a dict.";
      continue;
    }
    const auto* sources = dict->FindList("sources");
    const auto* destinations = dict->FindList("destinations");
    if (!sources || !destinations) {
      LOG(ERROR) << "Sources or destinations not found.";
      continue;
    }
    SourceDestinationEntry entry;
    entry.fs_sources = ValueListToFsTypes(sources);
    entry.fs_destinations = ValueListToFsTypes(destinations);
    if (entry.fs_sources.empty() || entry.fs_destinations.empty()) {
      LOG(ERROR) << "No valid sources or destinations found.";
      continue;
    }
    entry.id = ++(*id);
    source_destination_entries_.push_back(std::move(entry));
  }
}

std::set<SourceDestinationMatcherAsh::ID> SourceDestinationMatcherAsh::Match(
    content::BrowserContext* context,
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url) const {
  FsType source_fs_type = PathToFsType(context, source_url.path());
  FsType destination_fs_type = PathToFsType(context, destination_url.path());
  VLOG(1) << "source_fs_type = " << FsTypeToString(source_fs_type)
          << ", destination_fs_type = " << FsTypeToString(destination_fs_type);

  std::set<ID> matches;
  for (const SourceDestinationEntry& entry : source_destination_entries_) {
    if (entry.Matches(source_fs_type, destination_fs_type)) {
      matches.insert(entry.id);
    }
  }
  return matches;
}

SourceDestinationMatcherAsh::SourceDestinationEntry::SourceDestinationEntry() =
    default;
SourceDestinationMatcherAsh::SourceDestinationEntry::SourceDestinationEntry(
    const SourceDestinationEntry& other) = default;
SourceDestinationMatcherAsh::SourceDestinationEntry::SourceDestinationEntry(
    SourceDestinationEntry&& other) = default;
SourceDestinationMatcherAsh::SourceDestinationEntry::~SourceDestinationEntry() =
    default;

bool SourceDestinationMatcherAsh::SourceDestinationEntry::Matches(
    SourceDestinationMatcherAsh::FsType source_type,
    SourceDestinationMatcherAsh::FsType destination_type) const {
  return (fs_sources.count(FsType::kAny) || fs_sources.count(source_type)) &&
         (fs_destinations.count(FsType::kAny) ||
          fs_destinations.count(destination_type));
}

}  // namespace enterprise_connectors
