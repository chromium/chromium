// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/registry.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/file_system_provider/mount_path_util.h"
#include "chrome/browser/chromeos/file_system_provider/observer.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "storage/browser/file_system/external_mount_points.h"

namespace chromeos {
namespace file_system_provider {

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

Registry::~Registry() {
}

void Registry::RememberFileSystem(
    const ProvidedFileSystemInfo& file_system_info,
    const Watchers& watchers) {
  base::Value file_system(base::Value::Type::DICTIONARY);
  file_system.SetKey(kPrefKeyFileSystemId,
                     base::Value(file_system_info.file_system_id()));
  file_system.SetKey(kPrefKeyDisplayName,
                     base::Value(file_system_info.display_name()));
  file_system.SetKey(kPrefKeyWritable,
                     base::Value(file_system_info.writable()));
  file_system.SetKey(kPrefKeySupportsNotifyTag,
                     base::Value(file_system_info.supports_notify_tag()));
  file_system.SetKey(kPrefKeyOpenedFilesLimit,
                     base::Value(file_system_info.opened_files_limit()));
  // We don't need to write and read "persistent" field (in MountOptions) to
  // and from preference because all filesystems which are remembered must be
  // persistent.

  base::Value watchers_value(base::Value::Type::DICTIONARY);

  for (const auto& it : watchers) {
    base::Value watcher(base::Value::Type::DICTIONARY);
    watcher.SetKey(kPrefKeyWatcherEntryPath,
                   base::Value(it.second.entry_path.value()));
    watcher.SetKey(kPrefKeyWatcherRecursive, base::Value(it.second.recursive));
    watcher.SetKey(kPrefKeyWatcherLastTag, base::Value(it.second.last_tag));
    base::Value persistent_origins_value(base::Value::Type::LIST);
    for (const auto& subscriber_it : it.second.subscribers) {
      // Only persistent subscribers should be stored in persistent storage.
      // Other ones should not be restired after a restart.
      if (subscriber_it.second.persistent) {
        persistent_origins_value.Append(subscriber_it.first.spec());
      }
    }
    watcher.SetKey(kPrefKeyWatcherPersistentOrigins,
                   std::move(persistent_origins_value));
    watchers_value.SetKey(it.second.entry_path.value(), std::move(watcher));
  }
  file_system.SetKey(kPrefKeyWatchers, std::move(watchers_value));

  PrefService* const pref_service = profile_->GetPrefs();
  DCHECK(pref_service);

  DictionaryPrefUpdate dict_update(pref_service,
                                   prefs::kFileSystemProviderMounted);

  base::Value* file_systems_per_extension = dict_update->FindKeyOfType(
      file_system_info.provider_id().ToString(), base::Value::Type::DICTIONARY);
  if (!file_systems_per_extension) {
    file_systems_per_extension =
        dict_update->SetKey(file_system_info.provider_id().ToString(),
                            base::Value(base::Value::Type::DICTIONARY));
  }

  file_systems_per_extension->SetKey(file_system_info.file_system_id(),
                                     std::move(file_system));
}

void Registry::ForgetFileSystem(const ProviderId& provider_id,
                                const std::string& file_system_id) {
  PrefService* const pref_service = profile_->GetPrefs();
  DCHECK(pref_service);

  DictionaryPrefUpdate dict_update(pref_service,
                                   prefs::kFileSystemProviderMounted);

  base::DictionaryValue* file_systems_per_extension = NULL;
  if (!dict_update->GetDictionaryWithoutPathExpansion(
          provider_id.ToString(), &file_systems_per_extension))
    return;  // Nothing to forget.

  file_systems_per_extension->RemoveWithoutPathExpansion(file_system_id, NULL);
  if (file_systems_per_extension->empty())
    dict_update->Remove(provider_id.ToString(), NULL);
}

std::unique_ptr<Registry::RestoredFileSystems> Registry::RestoreFileSystems(
    const ProviderId& provider_id) {
  PrefService* const pref_service = profile_->GetPrefs();
  DCHECK(pref_service);

  const base::DictionaryValue* const file_systems =
      pref_service->GetDictionary(prefs::kFileSystemProviderMounted);
  DCHECK(file_systems);

  const base::DictionaryValue* file_systems_per_extension = NULL;
  if (!file_systems->GetDictionaryWithoutPathExpansion(
          provider_id.ToString(), &file_systems_per_extension)) {
    return base::WrapUnique(new RestoredFileSystems);  // Nothing to restore.
  }

  std::unique_ptr<RestoredFileSystems> restored_file_systems(
      new RestoredFileSystems);

  for (base::DictionaryValue::Iterator it(*file_systems_per_extension);
       !it.IsAtEnd();
       it.Advance()) {
    const base::Value* file_system_value = NULL;
    const base::DictionaryValue* file_system = NULL;
    file_systems_per_extension->GetWithoutPathExpansion(it.key(),
                                                        &file_system_value);
    DCHECK(file_system_value);

    std::string file_system_id;
    std::string display_name;
    bool writable = false;
    bool supports_notify_tag = false;
    int opened_files_limit = 0;

    // TODO(mtomasz): Move opened files limit to the mandatory list above in
    // M42.
    if ((!file_system_value->GetAsDictionary(&file_system) ||
         !file_system->GetStringWithoutPathExpansion(kPrefKeyFileSystemId,
                                                     &file_system_id) ||
         !file_system->GetStringWithoutPathExpansion(kPrefKeyDisplayName,
                                                     &display_name) ||
         !file_system->GetBooleanWithoutPathExpansion(kPrefKeyWritable,
                                                      &writable) ||
         !file_system->GetBooleanWithoutPathExpansion(kPrefKeySupportsNotifyTag,
                                                      &supports_notify_tag) ||
         file_system_id.empty() || display_name.empty()) ||
        // Optional fields.
        (file_system->GetIntegerWithoutPathExpansion(kPrefKeyOpenedFilesLimit,
                                                     &opened_files_limit) &&
         opened_files_limit < 0)) {
      LOG(ERROR)
          << "Malformed provided file system information in preferences.";
      continue;
    }

    MountOptions options;
    options.file_system_id = file_system_id;
    options.display_name = display_name;
    options.writable = writable;
    options.supports_notify_tag = supports_notify_tag;
    options.opened_files_limit = opened_files_limit;

    RestoredFileSystem restored_file_system;
    restored_file_system.provider_id = provider_id;
    restored_file_system.options = options;

    // Restore watchers. It's optional, since this field is new.
    const base::DictionaryValue* watchers = NULL;
    if (file_system->GetDictionaryWithoutPathExpansion(kPrefKeyWatchers,
                                                       &watchers)) {
      for (base::DictionaryValue::Iterator it(*watchers); !it.IsAtEnd();
           it.Advance()) {
        const base::Value* watcher_value = NULL;
        const base::DictionaryValue* watcher = NULL;
        watchers->GetWithoutPathExpansion(it.key(), &watcher_value);
        DCHECK(file_system_value);

        std::string entry_path;
        bool recursive = false;
        std::string last_tag;
        const base::ListValue* persistent_origins = NULL;

        if (!watcher_value->GetAsDictionary(&watcher) ||
            !watcher->GetStringWithoutPathExpansion(kPrefKeyWatcherEntryPath,
                                                    &entry_path) ||
            !watcher->GetBooleanWithoutPathExpansion(kPrefKeyWatcherRecursive,
                                                     &recursive) ||
            !watcher->GetStringWithoutPathExpansion(kPrefKeyWatcherLastTag,
                                                    &last_tag) ||
            !watcher->GetListWithoutPathExpansion(
                kPrefKeyWatcherPersistentOrigins, &persistent_origins) ||
            it.key() != entry_path || entry_path.empty() ||
            (!options.supports_notify_tag &&
             (!last_tag.empty() || persistent_origins->GetSize()))) {
          LOG(ERROR) << "Malformed watcher information in preferences.";
          continue;
        }

        Watcher restored_watcher;
        restored_watcher.entry_path =
            base::FilePath::FromUTF8Unsafe(entry_path);
        restored_watcher.recursive = recursive;
        restored_watcher.last_tag = last_tag;
        for (const auto& persistent_origin : *persistent_origins) {
          std::string origin;
          if (persistent_origin.GetAsString(&origin)) {
            LOG(ERROR) << "Malformed subscriber information in preferences.";
            continue;
          }
          const GURL origin_as_gurl(origin);
          restored_watcher.subscribers[origin_as_gurl].origin = origin_as_gurl;
          restored_watcher.subscribers[origin_as_gurl].persistent = true;
        }
        restored_file_system.watchers[WatcherKey(
            base::FilePath::FromUTF8Unsafe(entry_path), recursive)] =
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
  DictionaryPrefUpdate dict_update(pref_service,
                                   prefs::kFileSystemProviderMounted);

  // All of the following checks should not happen in healthy environment.
  // However, since they rely on storage, DCHECKs can't be used.
  base::DictionaryValue* file_systems_per_extension = NULL;
  base::DictionaryValue* file_system = NULL;
  base::DictionaryValue* watchers = NULL;
  base::DictionaryValue* watcher_value = NULL;
  if (!dict_update->GetDictionaryWithoutPathExpansion(
          file_system_info.provider_id().ToString(),
          &file_systems_per_extension) ||
      !file_systems_per_extension->GetDictionaryWithoutPathExpansion(
          file_system_info.file_system_id(), &file_system) ||
      !file_system->GetDictionaryWithoutPathExpansion(kPrefKeyWatchers,
                                                      &watchers) ||
      !watchers->GetDictionaryWithoutPathExpansion(watcher.entry_path.value(),
                                                   &watcher_value)) {
    // Broken preferences.
    LOG(ERROR) << "Broken preferences detected while updating a tag.";
    return;
  }

  watcher_value->SetKey(kPrefKeyWatcherLastTag, base::Value(watcher.last_tag));
}

}  // namespace file_system_provider
}  // namespace chromeos
