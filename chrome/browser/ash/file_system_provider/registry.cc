// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/registry.h"

#include <optional>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/browser/ash/file_system_provider/mount_path_util.h"
#include "chrome/browser/ash/file_system_provider/observer.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "storage/browser/file_system/external_mount_points.h"

namespace ash::file_system_provider {

const char kPrefKeyFileSystemId[] = "file-system-id";
const char kPrefKeyDisplayName[] = "display-name";
const char kPrefKeyWritable[] = "writable";
const char kPrefKeySupportsNotifyTag[] = "supports-notify-tag";
const char kPrefKeyWatchers[] = "watchers";
const char kPrefKeyWatcherEntryPath[] = "entry-path";
const char kPrefKeyWatcherRecursive[] = "recursive";
const char kPrefKeyWatcherLastTag[] = "last-tag";
const char kPrefKeyWatcherPersistentOrigins[] = "persistent-origins";
const char kPrefKeyOpenedFilesLimit[] = "opened-files-limit";

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kFileSystemProviderMounted);
}

Registry::Registry(Profile* profile) : profile_(profile) {
}

Registry::~Registry() = default;

void Registry::RememberFileSystem(
    const ProvidedFileSystemInfo& file_system_info,
    const Watchers& watchers) {
  base::Value::Dict file_system;
  file_system.Set(kPrefKeyFileSystemId, file_system_info.file_system_id());
  file_system.Set(kPrefKeyDisplayName, file_system_info.display_name());
  file_system.Set(kPrefKeyWritable, file_system_info.writable());
  file_system.Set(kPrefKeySupportsNotifyTag,
                  file_system_info.supports_notify_tag());
  file_system.Set(kPrefKeyOpenedFilesLimit,
                  file_system_info.opened_files_limit());
  // We don't need to write and read "persistent" field (in MountOptions) to
  // and from preference because all filesystems which are remembered must be
  // persistent.

  base::Value::Dict watchers_dict;

  for (const auto& it : watchers) {
    base::Value::Dict watcher;
    watcher.Set(kPrefKeyWatcherEntryPath, it.second.entry_path.value());
    watcher.Set(kPrefKeyWatcherRecursive, it.second.recursive);
    watcher.Set(kPrefKeyWatcherLastTag, it.second.last_tag);
    base::Value::List persistent_origins_value;
    for (const auto& subscriber_it : it.second.subscribers) {
      // Only persistent subscribers should be stored in persistent storage.
      // Other ones should not be restired after a restart.
      if (subscriber_it.second.persistent) {
        persistent_origins_value.Append(subscriber_it.first.spec());
      }
    }
    watcher.Set(kPrefKeyWatcherPersistentOrigins,
                std::move(persistent_origins_value));
    watchers_dict.Set(it.second.entry_path.value(), std::move(watcher));
  }
  file_system.Set(kPrefKeyWatchers, std::move(watchers_dict));

  PrefService* const pref_service = profile_->GetPrefs();
  DCHECK(pref_service);

  ScopedDictPrefUpdate dict_update(pref_service,
                                   prefs::kFileSystemProviderMounted);

  base::Value::Dict* file_systems_per_extension =
      dict_update->EnsureDict(file_system_info.provider_id().ToString());
  file_systems_per_extension->Set(file_system_info.file_system_id(),
                                  std::move(file_system));
}

void Registry::ForgetFileSystem(const ProviderId& provider_id,
                                const std::string& file_system_id) {
  PrefService* const pref_service = profile_->GetPrefs();
  DCHECK(pref_service);

  ScopedDictPrefUpdate dict_update(pref_service,
                                   prefs::kFileSystemProviderMounted);

  base::Value::Dict* file_systems_per_extension =
      dict_update->FindDict(provider_id.ToString());
  if (!file_systems_per_extension)
    return;  // Nothing to forget.

  file_systems_per_extension->Remove(file_system_id);
  if (file_systems_per_extension->empty())
    dict_update->Remove(provider_id.ToString());
}

std::unique_ptr<Registry::RestoredFileSystems> Registry::RestoreFileSystems(
    const ProviderId& provider_id) {
  PrefService* const pref_service = profile_->GetPrefs();
  DCHECK(pref_service);

  const base::Value::Dict& file_systems =
      pref_service->GetDict(prefs::kFileSystemProviderMounted);

  const base::Value::Dict* file_systems_per_extension =
      file_systems.FindDict(provider_id.ToString());
  if (!file_systems_per_extension) {
    return base::WrapUnique(new RestoredFileSystems);  // Nothing to restore.
  }

  std::unique_ptr<RestoredFileSystems> restored_file_systems(
      new RestoredFileSystems);

  for (const auto file_system_it : *file_systems_per_extension) {
    if (!file_system_it.second.is_dict()) {
      LOG(ERROR)
          << "Malformed provided file system information in preferences.";
      continue;
    }

    const base::Value::Dict& file_system = file_system_it.second.GetDict();

    const std::string* file_system_id =
        file_system.FindString(kPrefKeyFileSystemId);
    const std::string* display_name =
        file_system.FindString(kPrefKeyDisplayName);
    std::optional<bool> writable = file_system.FindBool(kPrefKeyWritable);
    std::optional<bool> supports_notify_tag =
        file_system.FindBool(kPrefKeySupportsNotifyTag);
    std::optional<int> opened_files_limit =
        file_system.FindInt(kPrefKeyOpenedFilesLimit);

    // TODO(mtomasz): Move opened files limit to the mandatory list above in
    // M42.
    if ((!file_system_id || !display_name || !writable ||
         !supports_notify_tag || file_system_id->empty() ||
         display_name->empty()) ||
        // Optional fields.
        (opened_files_limit.has_value() && opened_files_limit.value() < 0)) {
      LOG(ERROR)
          << "Malformed provided file system information in preferences.";
      continue;
    }

    MountOptions options;
    options.file_system_id = *file_system_id;
    options.display_name = *display_name;
    options.writable = writable.value();
    options.supports_notify_tag = supports_notify_tag.value();
    options.opened_files_limit = opened_files_limit.value_or(0);

    RestoredFileSystem restored_file_system;
    restored_file_system.provider_id = provider_id;
    restored_file_system.options = options;

    // Restore watchers. It's optional, since this field is new.
    const base::Value::Dict* watchers = file_system.FindDict(kPrefKeyWatchers);
    if (watchers) {
      for (const auto watcher_it : *watchers) {
        if (!watcher_it.second.is_dict()) {
          LOG(ERROR) << "Malformed watcher information in preferences.";
          continue;
        }

        const base::Value::Dict& watcher = watcher_it.second.GetDict();

        const std::string* entry_path =
            watcher.FindString(kPrefKeyWatcherEntryPath);
        std::optional<bool> recursive =
            watcher.FindBool(kPrefKeyWatcherRecursive);
        const std::string* last_tag =
            watcher.FindString(kPrefKeyWatcherLastTag);
        const base::Value::List* persistent_origins =
            watcher.FindList(kPrefKeyWatcherPersistentOrigins);

        if (!entry_path || !recursive || !last_tag || !persistent_origins ||
            watcher_it.first != *entry_path || entry_path->empty() ||
            (!options.supports_notify_tag &&
             (!last_tag->empty() || persistent_origins->size()))) {
          LOG(ERROR) << "Malformed watcher information in preferences.";
          continue;
        }

        Watcher restored_watcher;
        restored_watcher.entry_path =
            base::FilePath::FromUTF8Unsafe(*entry_path);
        restored_watcher.recursive = recursive.value();
        restored_watcher.last_tag = *last_tag;
        for (const auto& persistent_origin : *persistent_origins) {
          if (!persistent_origin.is_string()) {
            LOG(ERROR) << "Malformed subscriber information in preferences.";
            continue;
          }
          const GURL origin_as_gurl(persistent_origin.GetString());
          restored_watcher.subscribers[origin_as_gurl].origin = origin_as_gurl;
          restored_watcher.subscribers[origin_as_gurl].persistent = true;
        }
        restored_file_system.watchers[WatcherKey(
            base::FilePath::FromUTF8Unsafe(*entry_path), recursive.value())] =
            restored_watcher;
      }
    }
    restored_file_systems->push_back(restored_file_system);
  }

  return restored_file_systems;
}

void Registry::UpdateWatcherTag(const ProvidedFileSystemInfo& file_system_info,
                                const Watcher& watcher) {
  PrefService* const pref_service = profile_->GetPrefs();
  DCHECK(pref_service);

  // TODO(mtomasz): Consider optimizing it by moving information about watchers
  // or even file systems to leveldb.
  ScopedDictPrefUpdate dict_update(pref_service,
                                   prefs::kFileSystemProviderMounted);

  // All of the following checks should not happen in healthy environment.
  // However, since they rely on storage, DCHECKs can't be used.
  base::Value::Dict* file_systems_per_extension =
      dict_update->FindDict(file_system_info.provider_id().ToString());
  base::Value::Dict* file_system = nullptr;
  base::Value::Dict* watchers = nullptr;
  base::Value::Dict* watcher_value = nullptr;

  if (file_systems_per_extension) {
    file_system =
        file_systems_per_extension->FindDict(file_system_info.file_system_id());
  }
  if (file_system)
    watchers = file_system->FindDict(kPrefKeyWatchers);
  if (watchers)
    watcher_value = watchers->FindDict(watcher.entry_path.value());

  if (!watcher_value) {
    // Broken preferences.
    LOG(ERROR) << "Broken preferences detected while updating a tag.";
    return;
  }

  watcher_value->Set(kPrefKeyWatcherLastTag, watcher.last_tag);
}

}  // namespace ash::file_system_provider
