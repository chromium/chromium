// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_extension_helper.h"

#include <list>
#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/pending_extension_info.h"
#include "chrome/browser/extensions/pending_extension_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/crx_file/id_util.h"
#include "components/sync/model/string_ordinal.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/install_flag.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::Extension;
using extensions::ExtensionPrefs;
using extensions::Manifest;

const char kFakeExtensionPrefix[] = "fakeextension";

// static
SyncExtensionHelper* SyncExtensionHelper::GetInstance() {
  SyncExtensionHelper* instance = base::Singleton<SyncExtensionHelper>::get();
  instance->SetupIfNecessary(sync_datatype_helper::test());
  return instance;
}

SyncExtensionHelper::SyncExtensionHelper() = default;

SyncExtensionHelper::~SyncExtensionHelper() = default;

void SyncExtensionHelper::SetupIfNecessary(SyncTest* test) {
  if (setup_completed_) {
    return;
  }

  extension_name_prefix_ =
      kFakeExtensionPrefix + base::Uuid::GenerateRandomV4().AsLowercaseString();
  for (int i = 0; i < test->num_clients(); ++i) {
    SetupProfile(test->GetProfile(i));
  }
  if (test->UseVerifier()) {
    SetupProfile(test->verifier());
  }

  setup_completed_ = true;
}

std::string SyncExtensionHelper::InstallExtension(Profile* profile,
                                                  const std::string& name,
                                                  Manifest::Type type) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  scoped_refptr<Extension> extension = GetExtension(profile, name, type);
  if (!extension.get()) {
    NOTREACHED_IN_MIGRATION() << "Could not install extension " << name;
    return std::string();
  }
  extensions::ExtensionSystem::Get(profile)
      ->extension_service()
      ->OnExtensionInstalled(extension.get(), syncer::StringOrdinal(),
                             extensions::kInstallFlagInstallImmediately);
  return extension->id();
}

void SyncExtensionHelper::UninstallExtension(Profile* profile,
                                             const std::string& name) {
  extensions::ExtensionSystem::Get(profile)
      ->extension_service()
      ->UninstallExtension(crx_file::id_util::GenerateId(name),
                           extensions::UNINSTALL_REASON_SYNC,
                           nullptr /* error */);
}

std::vector<std::string> SyncExtensionHelper::GetInstalledExtensionNames(
    Profile* profile) const {
  std::vector<std::string> names;

  const extensions::ExtensionSet extensions =
      extensions::ExtensionRegistry::Get(profile)
          ->GenerateInstalledExtensionsSet();
  for (const auto& extension : extensions) {
    names.push_back(extension->name());
  }

  return names;
}

void SyncExtensionHelper::EnableExtension(Profile* profile,
                                          const std::string& name) {
  extensions::ExtensionSystem::Get(profile)
      ->extension_service()
      ->EnableExtension(crx_file::id_util::GenerateId(name));
}

void SyncExtensionHelper::DisableExtension(Profile* profile,
                                           const std::string& name) {
  extensions::ExtensionSystem::Get(profile)
      ->extension_service()
      ->DisableExtension(crx_file::id_util::GenerateId(name),
                         extensions::disable_reason::DISABLE_USER_ACTION);
}

bool SyncExtensionHelper::IsExtensionEnabled(Profile* profile,
                                             const std::string& name) const {
  return extensions::ExtensionSystem::Get(profile)
      ->extension_service()
      ->IsExtensionEnabled(crx_file::id_util::GenerateId(name));
}

void SyncExtensionHelper::IncognitoEnableExtension(Profile* profile,
                                                   const std::string& name) {
  extensions::util::SetIsIncognitoEnabled(crx_file::id_util::GenerateId(name),
                                          profile, true);
}

void SyncExtensionHelper::IncognitoDisableExtension(Profile* profile,
                                                    const std::string& name) {
  extensions::util::SetIsIncognitoEnabled(crx_file::id_util::GenerateId(name),
                                          profile, false);
}

bool SyncExtensionHelper::IsIncognitoEnabled(Profile* profile,
                                             const std::string& name) const {
  return extensions::util::IsIncognitoEnabled(
      crx_file::id_util::GenerateId(name), profile);
}

bool SyncExtensionHelper::IsExtensionPendingInstallForSync(
    Profile* profile,
    const std::string& id) const {
  const extensions::PendingExtensionManager* pending_extension_manager =
      extensions::ExtensionSystem::Get(profile)
          ->extension_service()
          ->pending_extension_manager();
  const extensions::PendingExtensionInfo* info =
      pending_extension_manager->GetById(id);
  if (!info) {
    return false;
  }
  return info->is_from_sync();
}

void SyncExtensionHelper::InstallExtensionsPendingForSync(Profile* profile) {
  // TODO(akalin): Mock out the servers that the extensions auto-update
  // mechanism talk to so as to more closely match what actually happens.
  // Background networking will need to be re-enabled for extensions tests.

  // We make a copy here since InstallExtension() removes the
  // extension from the extensions service's copy.
  const extensions::PendingExtensionManager* pending_extension_manager =
      extensions::ExtensionSystem::Get(profile)
          ->extension_service()
          ->pending_extension_manager();

  std::list<std::string> pending_crx_ids =
      pending_extension_manager->GetPendingIdsForUpdateCheck();

  const extensions::PendingExtensionInfo* info = nullptr;
  for (const std::string& pending_crx_id : pending_crx_ids) {
    ASSERT_TRUE(info = pending_extension_manager->GetById(pending_crx_id));
    if (!info->is_from_sync()) {
      continue;
    }

    StringMap::const_iterator iter = id_to_name_.find(pending_crx_id);
    if (iter == id_to_name_.end()) {
      ADD_FAILURE() << "Could not get name for id " << pending_crx_id
                    << " (profile = " << profile->GetDebugName() << ")";
      continue;
    }
    TypeMap::const_iterator iter2 = id_to_type_.find(pending_crx_id);
    if (iter2 == id_to_type_.end()) {
      ADD_FAILURE() << "Could not get type for id " << pending_crx_id
                    << " (profile = " << profile->GetDebugName() << ")";
    }
    InstallExtension(profile, iter->second, iter2->second);
  }
}

SyncExtensionHelper::ExtensionStateMap SyncExtensionHelper::GetExtensionStates(
    Profile* profile) {
  const std::string& profile_debug_name = profile->GetDebugName();

  ExtensionStateMap extension_state_map;

  const extensions::ExtensionSet extensions =
      extensions::ExtensionRegistry::Get(profile)
          ->GenerateInstalledExtensionsSet();

  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  for (const scoped_refptr<const Extension>& extension : extensions) {
    const std::string& id = extension->id();
    extension_state_map[id] = {
        .enabled_state = extension_service->IsExtensionEnabled(id)
                             ? ExtensionState::ENABLED
                             : ExtensionState::DISABLED,
        .disable_reasons = ExtensionPrefs::Get(profile)->GetDisableReasons(id),
        .incognito_enabled = extensions::util::IsIncognitoEnabled(id, profile)};

    DVLOG(2) << "Extension " << id << " in profile " << profile_debug_name
             << " is "
             << (extension_service->IsExtensionEnabled(id) ? "enabled"
                                                           : "disabled");
  }

  const extensions::PendingExtensionManager* pending_extension_manager =
      extension_service->pending_extension_manager();

  std::list<std::string> pending_crx_ids =
      pending_extension_manager->GetPendingIdsForUpdateCheck();

  for (const std::string& id : pending_crx_ids) {
    extension_state_map[id] = {
        .enabled_state = ExtensionState::PENDING,
        .disable_reasons = ExtensionPrefs::Get(profile)->GetDisableReasons(id),
        .incognito_enabled = extensions::util::IsIncognitoEnabled(id, profile)};
    DVLOG(2) << "Extension " << id << " in profile " << profile_debug_name
             << " is pending";
  }

  return extension_state_map;
}

bool SyncExtensionHelper::ExtensionStatesMatch(Profile* profile1,
                                               Profile* profile2) {
  const ExtensionStateMap& state_map1 = GetExtensionStates(profile1);
  const ExtensionStateMap& state_map2 = GetExtensionStates(profile2);
  if (state_map1.size() != state_map2.size()) {
    DVLOG(1) << "Number of extensions for profile " << profile1->GetDebugName()
             << " does not match profile " << profile2->GetDebugName();
    return false;
  }

  auto it1 = state_map1.begin();
  auto it2 = state_map2.begin();
  while (it1 != state_map1.end()) {
    const auto& [app_id1, app_state1] = *it1;
    const auto& [app_id2, app_state2] = *it2;
    if (app_id1 != app_id2) {
      DVLOG(1) << "Extensions for profile " << profile1->GetDebugName()
               << " do not match profile " << profile2->GetDebugName();
      return false;
    } else if (app_state1 != app_state2) {
      DVLOG(1) << "Extension states for profile " << profile1->GetDebugName()
               << " do not match profile " << profile2->GetDebugName();
      return false;
    }
    ++it1;
    ++it2;
  }
  return true;
}

std::string SyncExtensionHelper::CreateFakeExtensionName(int index) {
  return extension_name_prefix_ + base::NumberToString(index);
}

bool SyncExtensionHelper::ExtensionNameToIndex(const std::string& name,
                                               int* index) {
  if (!(base::StartsWith(name, extension_name_prefix_,
                         base::CompareCase::SENSITIVE) &&
        base::StringToInt(name.substr(extension_name_prefix_.size()), index))) {
    LOG(WARNING) << "Unable to convert extension name \"" << name
                 << "\" to index";
    return false;
  }
  return true;
}

void SyncExtensionHelper::SetupProfile(Profile* profile) {
  extensions::ExtensionSystem::Get(profile)->InitForRegularProfile(
      true /* extensions_enabled */);
  profile_extensions_.insert(make_pair(profile, ExtensionNameMap()));
}

namespace {

std::string NameToPublicKey(const std::string& name) {
  std::string public_key;
  std::string pem;
  EXPECT_TRUE(Extension::ProducePEM(name, &pem) &&
              Extension::FormatPEMForFileOutput(pem, &public_key,
                                                true /* is_public */));
  return public_key;
}

// TODO(akalin): Somehow unify this with MakeExtension() in
// extension_util_unittest.cc.
scoped_refptr<Extension> CreateExtension(const base::FilePath& base_dir,
                                         const std::string& name,
                                         Manifest::Type type) {
  base::Value::Dict source;
  source.SetByDottedPath(extensions::manifest_keys::kName, name);
  const std::string& public_key = NameToPublicKey(name);
  source.SetByDottedPath(extensions::manifest_keys::kPublicKey, public_key);
  source.SetByDottedPath(extensions::manifest_keys::kVersion, "0.0.0.0");
  source.SetByDottedPath(extensions::manifest_keys::kManifestVersion, 2);
  switch (type) {
    case Manifest::TYPE_EXTENSION:
      // Do nothing.
      break;
    case Manifest::TYPE_THEME:
      source.SetByDottedPath(extensions::manifest_keys::kTheme,
                             base::Value::Dict());
      break;
    case Manifest::TYPE_HOSTED_APP:
    case Manifest::TYPE_LEGACY_PACKAGED_APP:
      source.SetByDottedPath(extensions::manifest_keys::kApp,
                             base::Value::Dict());
      source.SetByDottedPath(extensions::manifest_keys::kLaunchWebURL,
                             "http://www.example.com");
      break;
    case Manifest::TYPE_PLATFORM_APP: {
      source.SetByDottedPath(extensions::manifest_keys::kApp,
                             base::Value::Dict());
      source.SetByDottedPath(extensions::manifest_keys::kPlatformAppBackground,
                             base::Value::Dict());
      base::Value::List scripts;
      scripts.Append("main.js");
      source.SetByDottedPath(
          extensions::manifest_keys::kPlatformAppBackgroundScripts,
          std::move(scripts));
      break;
    }
    default:
      ADD_FAILURE();
      return nullptr;
  }
  const base::FilePath sub_dir = base::FilePath().AppendASCII(name);
  base::FilePath extension_dir;
  if (!base::PathExists(base_dir) && !base::CreateDirectory(base_dir)) {
    ADD_FAILURE();
    return nullptr;
  }
  if (!base::CreateTemporaryDirInDir(base_dir, sub_dir.value(),
                                     &extension_dir)) {
    ADD_FAILURE();
    return nullptr;
  }
  std::string error;
  scoped_refptr<Extension> extension = Extension::Create(
      extension_dir, extensions::mojom::ManifestLocation::kInternal, source,
      Extension::NO_FLAGS, &error);
  if (!error.empty()) {
    ADD_FAILURE() << error;
    return nullptr;
  }
  if (!extension.get()) {
    ADD_FAILURE();
    return nullptr;
  }
  if (extension->name() != name) {
    EXPECT_EQ(name, extension->name());
    return nullptr;
  }
  if (extension->GetType() != type) {
    EXPECT_EQ(type, extension->GetType());
    return nullptr;
  }
  return extension;
}

}  // namespace

scoped_refptr<Extension> SyncExtensionHelper::GetExtension(
    Profile* profile,
    const std::string& name,
    Manifest::Type type) {
  if (name.empty()) {
    ADD_FAILURE();
    return nullptr;
  }
  auto it = profile_extensions_.find(profile);
  if (it == profile_extensions_.end()) {
    ADD_FAILURE();
    return nullptr;
  }
  ExtensionNameMap::const_iterator it2 = it->second.find(name);
  if (it2 != it->second.end()) {
    return it2->second;
  }

  scoped_refptr<Extension> extension =
      CreateExtension(extensions::ExtensionSystem::Get(profile)
                          ->extension_service()
                          ->install_directory(),
                      name, type);
  if (!extension.get()) {
    ADD_FAILURE();
    return nullptr;
  }
  const std::string& expected_id = crx_file::id_util::GenerateId(name);
  if (extension->id() != expected_id) {
    EXPECT_EQ(expected_id, extension->id());
    return nullptr;
  }
  DVLOG(2) << "created extension with name = " << name
           << ", id = " << expected_id;
  (it->second)[name] = extension;
  id_to_name_[expected_id] = name;
  id_to_type_[expected_id] = type;
  return extension;
}
