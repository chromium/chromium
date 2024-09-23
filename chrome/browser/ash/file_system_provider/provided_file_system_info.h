// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_PROVIDED_FILE_SYSTEM_INFO_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_PROVIDED_FILE_SYSTEM_INFO_H_

#include <string>

#include "base/files/file_path.h"
#include "chrome/browser/ash/file_system_provider/icon_set.h"
#include "chrome/common/extensions/api/file_system_provider_capabilities/file_system_provider_capabilities_handler.h"
#include "extensions/common/extension_id.h"

namespace ash::file_system_provider {

// Options for creating the provided file system info.
struct MountOptions {
  MountOptions();

  // Only mandatory fields.
  MountOptions(const std::string& file_system_id,
               const std::string& display_name);

  MountOptions(const MountOptions& source);
  MountOptions& operator=(const MountOptions& source);

  std::string file_system_id;
  std::string display_name;
  bool writable;
  bool supports_notify_tag;
  int opened_files_limit;
  bool persistent;
};

class ProviderId {
 public:
  enum ProviderType : uint32_t { EXTENSION, NATIVE, INVALID };
  ProviderId();

  static ProviderId CreateFromExtensionId(
      const extensions::ExtensionId& extension_id);
  static ProviderId CreateFromNativeId(const std::string& native_id);
  static ProviderId FromString(const std::string& provider_id);

  const extensions::ExtensionId& GetExtensionId() const;
  const std::string& GetNativeId() const;
  std::string ToString() const;
  ProviderType GetType() const;

  bool operator==(const ProviderId& other) const;
  bool operator!=(const ProviderId& other) const;
  bool operator<(const ProviderId& other) const;

 private:
  ProviderId(const std::string& internal_id, ProviderType provider_type);

  std::string internal_id_;
  ProviderType type_;
};

// The type of content cache that is used for the individual provider.
// TODO(b/317137739): Move this value to
// file_system_provider_capabilities_handler.h` once the
// chrome.fileSystemProvider manifest exposes this value.
enum class CacheType { LRU, NONE };

// Contains information about the provided file system instance.
class ProvidedFileSystemInfo {
 public:
  ProvidedFileSystemInfo();

  ProvidedFileSystemInfo(const ProviderId& provider_id,
                         const MountOptions& mount_options,
                         const base::FilePath& mount_path,
                         bool configurable,
                         bool watchable,
                         extensions::FileSystemProviderSource source,
                         const IconSet& icon_set,
                         CacheType cache_type = CacheType::NONE);

  // TODO(mtomasz): Remove this constructor. Callers should be using
  // provider id, not extension id.
  ProvidedFileSystemInfo(const extensions::ExtensionId& extension_id,
                         const MountOptions& mount_options,
                         const base::FilePath& mount_path,
                         bool configurable,
                         bool watchable,
                         extensions::FileSystemProviderSource source,
                         const IconSet& icon_set,
                         CacheType cache_type = CacheType::NONE);

  ProvidedFileSystemInfo(const ProvidedFileSystemInfo& other);

  ~ProvidedFileSystemInfo();

  const ProviderId& provider_id() const { return provider_id_; }
  const std::string& file_system_id() const { return file_system_id_; }
  const std::string& display_name() const { return display_name_; }
  bool writable() const { return writable_; }
  bool supports_notify_tag() const { return supports_notify_tag_; }
  int opened_files_limit() const { return opened_files_limit_; }
  const base::FilePath& mount_path() const { return mount_path_; }
  bool configurable() const { return configurable_; }
  bool watchable() const { return watchable_; }
  extensions::FileSystemProviderSource source() const { return source_; }
  const IconSet& icon_set() const { return icon_set_; }
  CacheType cache_type() const { return cache_type_; }

  bool operator==(const ProvidedFileSystemInfo& other) const;

 private:
  // ID of the provider supplying this file system.
  ProviderId provider_id_;

  // ID of the file system.
  std::string file_system_id_;

  // Name of the file system, can be rendered in the UI.
  std::string display_name_;

  // Whether the file system is writable or just read-only.
  bool writable_;

  // Supports tags for file/directory change notifications.
  bool supports_notify_tag_;

  // Limit of opened files in parallel. If unlimited, then 0.
  int opened_files_limit_;

  // Mount path of the underlying file system.
  base::FilePath mount_path_;

  // TODO(mtomasz): Move all of the following 5 members to a separate structure
  // called ProviderInfo, as this is not supposed to be customizable per
  // file systems. It actually must not be. These are not properties of
  // a file system, but of a provider.

  // Whether the file system is configurable.
  bool configurable_;

  // Whether the file system is watchable.
  bool watchable_;

  // Source of the file system's data.
  extensions::FileSystemProviderSource source_;

  // Icon set for the file system.
  IconSet icon_set_;

  // The type of content cache that this file system leverages for eviction.
  CacheType cache_type_;
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_PROVIDED_FILE_SYSTEM_INFO_H_
