// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"

#include "base/check_op.h"

namespace ash::file_system_provider {

ProviderId::ProviderId(const std::string& internal_id,
                       ProviderType provider_type)
    : internal_id_(internal_id), type_(provider_type) {}

ProviderId::ProviderId() : type_(INVALID) {}

// static
ProviderId ProviderId::CreateFromExtensionId(
    const extensions::ExtensionId& extension_id) {
  return ProviderId(extension_id, EXTENSION);
}

// static
ProviderId ProviderId::CreateFromNativeId(const std::string& native_id) {
  return ProviderId(native_id, NATIVE);
}

ProviderId ProviderId::FromString(const std::string& str) {
  if (str.length() == 0)
    return ProviderId();  // Invalid.

  if (str[0] == '@')
    return ProviderId::CreateFromNativeId(str.substr(1));

  return ProviderId::CreateFromExtensionId(str);
}

const extensions::ExtensionId& ProviderId::GetExtensionId() const {
  CHECK_EQ(EXTENSION, type_);
  return internal_id_;
}

const std::string& ProviderId::GetNativeId() const {
  CHECK_EQ(NATIVE, type_);
  return internal_id_;
}

ProviderId::ProviderType ProviderId::GetType() const {
  return type_;
}

// Returns the internal_id_ for extensions for  backwards compatibility,
// Adds '@' for native ids to avoid collisions.
std::string ProviderId::ToString() const {
  switch (type_) {
    case EXTENSION:
      return internal_id_;
    case NATIVE:
      return std::string("@") + internal_id_;
    case INVALID:
      return "";
  }
}

bool ProviderId::operator==(const ProviderId& other) const {
  return type_ == other.type_ && internal_id_ == other.internal_id_;
}

bool ProviderId::operator!=(const ProviderId& other) const {
  return !operator==(other);
}

bool ProviderId::operator<(const ProviderId& other) const {
  return std::tie(type_, internal_id_) <
         std::tie(other.type_, other.internal_id_);
}

MountOptions::MountOptions()
    : writable(false),
      supports_notify_tag(false),
      opened_files_limit(0),
      persistent(true) {}

MountOptions::MountOptions(const std::string& file_system_id,
                           const std::string& display_name)
    : file_system_id(file_system_id),
      display_name(display_name),
      writable(false),
      supports_notify_tag(false),
      opened_files_limit(0),
      persistent(true) {}

MountOptions::MountOptions(const MountOptions& source) = default;

MountOptions& MountOptions::operator=(const MountOptions& source) = default;

ProvidedFileSystemInfo::ProvidedFileSystemInfo()
    : writable_(false),
      supports_notify_tag_(false),
      configurable_(false),
      watchable_(false),
      source_(extensions::SOURCE_FILE),
      cache_type_(CacheType::NONE) {}

ProvidedFileSystemInfo::ProvidedFileSystemInfo(
    const ProviderId& provider_id,
    const MountOptions& mount_options,
    const base::FilePath& mount_path,
    bool configurable,
    bool watchable,
    extensions::FileSystemProviderSource source,
    const IconSet& icon_set,
    CacheType cache_type)
    : provider_id_(provider_id),
      file_system_id_(mount_options.file_system_id),
      display_name_(mount_options.display_name),
      writable_(mount_options.writable),
      supports_notify_tag_(mount_options.supports_notify_tag),
      opened_files_limit_(mount_options.opened_files_limit),
      mount_path_(mount_path),
      configurable_(configurable),
      watchable_(watchable),
      source_(source),
      icon_set_(icon_set),
      cache_type_(cache_type) {
  DCHECK_LE(0, mount_options.opened_files_limit);
}

ProvidedFileSystemInfo::ProvidedFileSystemInfo(
    const extensions::ExtensionId& extension_id,
    const MountOptions& mount_options,
    const base::FilePath& mount_path,
    bool configurable,
    bool watchable,
    extensions::FileSystemProviderSource source,
    const IconSet& icon_set,
    CacheType cache_type)
    : ProvidedFileSystemInfo(ProviderId::CreateFromExtensionId(extension_id),
                             mount_options,
                             mount_path,
                             configurable,
                             watchable,
                             source,
                             icon_set,
                             cache_type) {}

ProvidedFileSystemInfo::ProvidedFileSystemInfo(
    const ProvidedFileSystemInfo& other) = default;

ProvidedFileSystemInfo::~ProvidedFileSystemInfo() = default;

bool ProvidedFileSystemInfo::operator==(
    const ProvidedFileSystemInfo& other) const {
  return provider_id() == other.provider_id() &&
         file_system_id() == other.file_system_id() &&
         display_name() == other.display_name() &&
         writable() == other.writable() &&
         supports_notify_tag() == other.supports_notify_tag() &&
         opened_files_limit() == other.opened_files_limit() &&
         mount_path() == other.mount_path() &&
         configurable() == other.configurable() &&
         watchable() == other.watchable() && source() == other.source() &&
         icon_set() == other.icon_set() && cache_type() == other.cache_type();
}

}  // namespace ash::file_system_provider
