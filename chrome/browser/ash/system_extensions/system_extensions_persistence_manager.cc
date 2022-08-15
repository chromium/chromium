// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_extensions/system_extensions_persistence_manager.h"

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace ash {

namespace prefs {
static constexpr char kPersistedSystemExtensions[] =
    "system_extensions.persisted";
}  // namespace prefs

namespace {
static constexpr char kSystemExtensionManifest[] = "manifest";
}  // namespace

// static
void SystemExtensionsPersistenceManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kPersistedSystemExtensions);
}

SystemExtensionsPersistenceManager::SystemExtensionsPersistenceManager(
    Profile* profile)
    : profile_(profile) {}

SystemExtensionsPersistenceManager::~SystemExtensionsPersistenceManager() =
    default;

void SystemExtensionsPersistenceManager::Persist(
    const SystemExtension& system_extension) {
  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              prefs::kPersistedSystemExtensions);
  base::Value::Dict& persisted_system_extensions_map = update->GetDict();

  base::Value::Dict persisted_system_extension;
  persisted_system_extension.Set(kSystemExtensionManifest,
                                 system_extension.manifest.Clone());

  persisted_system_extensions_map.Set(
      SystemExtension::IdToString(system_extension.id),
      std::move(persisted_system_extension));
}

void SystemExtensionsPersistenceManager::Delete(
    const SystemExtensionId& system_extension_id) {
  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              prefs::kPersistedSystemExtensions);
  update->GetDict().Remove(SystemExtension::IdToString(system_extension_id));
}

absl::optional<SystemExtensionPersistenceInfo>
SystemExtensionsPersistenceManager::Get(
    const SystemExtensionId& system_extension_id) {
  auto* prefs = profile_->GetPrefs();
  const base::Value::Dict& persisted_system_extensions_map =
      prefs->GetValueDict(prefs::kPersistedSystemExtensions);

  const base::Value::Dict* persisted_system_extension =
      persisted_system_extensions_map.FindDict(
          SystemExtension::IdToString(system_extension_id));
  if (!persisted_system_extension)
    return absl::nullopt;

  const base::Value::Dict* manifest_pref =
      persisted_system_extension->FindDict(kSystemExtensionManifest);
  if (!manifest_pref)
    return absl::nullopt;

  absl::optional<SystemExtensionPersistenceInfo> info;
  info.emplace();
  info->id = system_extension_id;
  info->manifest = manifest_pref->Clone();

  return info;
}

std::vector<SystemExtensionPersistenceInfo>
SystemExtensionsPersistenceManager::GetAll() {
  std::vector<SystemExtensionPersistenceInfo> infos;

  auto* prefs = profile_->GetPrefs();
  const base::Value::Dict& persisted_system_extensions_map =
      prefs->GetValueDict(prefs::kPersistedSystemExtensions);
  for (const auto [id_str, _] : persisted_system_extensions_map) {
    absl::optional<SystemExtensionId> id = SystemExtension::StringToId(id_str);
    if (!id)
      continue;

    absl::optional<SystemExtensionPersistenceInfo> info = Get(id.value());
    if (!info)
      continue;

    infos.emplace_back(std::move(info.value()));
  }

  return infos;
}

}  // namespace ash
