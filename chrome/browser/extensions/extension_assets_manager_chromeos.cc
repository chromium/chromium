// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_assets_manager_chromeos.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/not_fatal_until.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_url_handlers.h"

using content::BrowserThread;

namespace extensions {
namespace {

// Path to shared extensions install dir.
const char kSharedExtensionsDir[] = "/var/cache/shared_extensions";

// Shared install dir overrider for tests only.
static const base::FilePath* g_shared_install_dir_override = nullptr;

// This helper class lives on UI thread only. Main purpose of this class is to
// track shared installation in progress between multiple profiles.
class ExtensionAssetsManagerHelper {
 public:
  // Info about pending install request.
  struct PendingInstallInfo {
    base::FilePath unpacked_extension_root;
    base::FilePath local_install_dir;
    raw_ptr<Profile> profile;
    ExtensionAssetsManager::InstallExtensionCallback callback;
  };
  using PendingInstallList = std::vector<PendingInstallInfo>;

  ExtensionAssetsManagerHelper(const ExtensionAssetsManagerHelper&) = delete;
  ExtensionAssetsManagerHelper& operator=(const ExtensionAssetsManagerHelper&) =
      delete;

  static ExtensionAssetsManagerHelper* GetInstance() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    return base::Singleton<ExtensionAssetsManagerHelper>::get();
  }

  // Remember that shared install is in progress. Return true if there is no
  // other installs for given id and version.
  bool RecordSharedInstall(
      const std::string& id,
      const std::string& version,
      const base::FilePath& unpacked_extension_root,
      const base::FilePath& local_install_dir,
      Profile* profile,
      ExtensionAssetsManager::InstallExtensionCallback callback) {
    PendingInstallInfo install_info;
    install_info.unpacked_extension_root = unpacked_extension_root;
    install_info.local_install_dir = local_install_dir;
    install_info.profile = profile;
    install_info.callback = std::move(callback);

    std::vector<PendingInstallInfo>& callbacks =
        install_queue_[InstallQueue::key_type(id, version)];
    callbacks.push_back(std::move(install_info));

    return callbacks.size() == 1;
  }

  // Remove record about shared installation in progress and return
  // |pending_installs|.
  void SharedInstallDone(const std::string& id,
                         const std::string& version,
                         PendingInstallList* pending_installs) {
    InstallQueue::iterator it = install_queue_.find(
        InstallQueue::key_type(id, version));
    CHECK(it != install_queue_.end(), base::NotFatalUntil::M130);
    pending_installs->swap(it->second);
    install_queue_.erase(it);
  }

 private:
  friend struct base::DefaultSingletonTraits<ExtensionAssetsManagerHelper>;

  ExtensionAssetsManagerHelper() = default;
  ~ExtensionAssetsManagerHelper() = default;

  // Extension ID + version pair.
  using InstallItem = std::pair<std::string, std::string>;

  // Queue of pending installs in progress.
  using InstallQueue = std::map<InstallItem, std::vector<PendingInstallInfo>>;

  InstallQueue install_queue_;
};

}  // namespace

const char ExtensionAssetsManagerChromeOS::kSharedExtensions[] =
    "SharedExtensions";

const char ExtensionAssetsManagerChromeOS::kSharedExtensionPath[] = "path";

const char ExtensionAssetsManagerChromeOS::kSharedExtensionUsers[] = "users";

ExtensionAssetsManagerChromeOS::ExtensionAssetsManagerChromeOS() { }

ExtensionAssetsManagerChromeOS::~ExtensionAssetsManagerChromeOS() {
  if (g_shared_install_dir_override) {
    delete g_shared_install_dir_override;
    g_shared_install_dir_override = nullptr;
  }
}

// static
ExtensionAssetsManagerChromeOS* ExtensionAssetsManagerChromeOS::GetInstance() {
  return base::Singleton<ExtensionAssetsManagerChromeOS>::get();
}

// static
void ExtensionAssetsManagerChromeOS::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kSharedExtensions);
}

void ExtensionAssetsManagerChromeOS::InstallExtension(
    const Extension* extension,
    const base::FilePath& unpacked_extension_root,
    const base::FilePath& local_install_dir,
    Profile* profile,
    InstallExtensionCallback callback,
    bool updates_from_webstore_or_empty_update_url) {
  if (!CanShareAssets(extension, unpacked_extension_root,
                      updates_from_webstore_or_empty_update_url)) {
    InstallLocalExtension(extension->id(), extension->VersionString(),
                          unpacked_extension_root, local_install_dir,
                          std::move(callback));
    return;
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ExtensionAssetsManagerChromeOS::CheckSharedExtension,
                     extension->id(), extension->VersionString(),
                     unpacked_extension_root, local_install_dir, profile,
                     std::move(callback)));
}

void ExtensionAssetsManagerChromeOS::UninstallExtension(
    const std::string& id,
    const std::string& profile_user_name,
    const base::FilePath& extensions_install_dir,
    const base::FilePath& extension_dir_to_delete,
    const base::FilePath& profile_dir) {
  if (extensions_install_dir.IsParent(extension_dir_to_delete)) {
    file_util::UninstallExtension(profile_dir, extensions_install_dir,
                                  extension_dir_to_delete);
    return;
  }

  if (GetSharedInstallDir().IsParent(extension_dir_to_delete)) {
    // In some test extensions installed outside local_install_dir emulate
    // previous behavior that just do nothing in this case.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ExtensionAssetsManagerChromeOS::MarkSharedExtensionUnused, id,
            profile_user_name));
  }
}

// static
base::FilePath ExtensionAssetsManagerChromeOS::GetSharedInstallDir() {
  if (g_shared_install_dir_override)
    return *g_shared_install_dir_override;
  else
    return base::FilePath(kSharedExtensionsDir);
}

// static
bool ExtensionAssetsManagerChromeOS::IsSharedInstall(
    const Extension* extension) {
  return GetSharedInstallDir().IsParent(extension->path());
}

// static
bool ExtensionAssetsManagerChromeOS::CleanUpSharedExtensions(
    std::multimap<std::string, base::FilePath>* live_extension_paths) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  PrefService* local_state = g_browser_process->local_state();
  // It happens in many unit tests.
  if (!local_state)
    return false;

  ScopedDictPrefUpdate shared_extensions(local_state, kSharedExtensions);
  base::Value::Dict& shared_extension_dict = shared_extensions.Get();

  std::vector<std::string> extensions;
  extensions.reserve(shared_extension_dict.size());
  for (const auto it : shared_extension_dict)
    extensions.push_back(it.first);

  for (const std::string& id : extensions) {
    base::Value::Dict* extension_info = shared_extension_dict.FindDict(id);
    if (!extension_info) {
      NOTREACHED_IN_MIGRATION();
      return false;
    }
    if (!CleanUpExtension(id, *extension_info, live_extension_paths)) {
      return false;
    }
    if (extension_info->empty())
      shared_extension_dict.Remove(id);
  }

  return true;
}

// static
void ExtensionAssetsManagerChromeOS::SetSharedInstallDirForTesting(
    const base::FilePath& install_dir) {
  DCHECK(!g_shared_install_dir_override);
  g_shared_install_dir_override = new base::FilePath(install_dir);
}

// static
bool ExtensionAssetsManagerChromeOS::CanShareAssets(
    const Extension* extension,
    const base::FilePath& unpacked_extension_root,
    bool updates_from_webstore_or_empty_update_url) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kEnableExtensionAssetsSharing)) {
    return false;
  }

  // TODO(crbug.com/40742161): Investigate why do we allow sharing assets in
  // case of empty update URL and if the empty update URL is not required,
  // update this to consider only the updates from webstore.
  if (!updates_from_webstore_or_empty_update_url)
    return false;

  // Chrome caches crx files for installed by default apps so sharing assets is
  // also possible. User specific apps should be excluded to not expose apps
  // unique for the user outside of user's cryptohome.
  return Manifest::IsExternalLocation(extension->location());
}

// static
void ExtensionAssetsManagerChromeOS::CheckSharedExtension(
    const std::string& id,
    const std::string& version,
    const base::FilePath& unpacked_extension_root,
    const base::FilePath& local_install_dir,
    Profile* profile,
    InstallExtensionCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const std::string& user_id = profile->GetProfileUserName();
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (!user_manager) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  if (user_manager->IsUserNonCryptohomeDataEphemeral(
          AccountId::FromUserEmail(user_id)) ||
      !user_manager->IsLoggedInAsUserWithGaiaAccount()) {
    // Don't cache anything in shared location for ephemeral user or special
    // user types.
    GetExtensionFileTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ExtensionAssetsManagerChromeOS::InstallLocalExtension,
                       id, version, unpacked_extension_root, local_install_dir,
                       std::move(callback)));
    return;
  }

  PrefService* local_state = g_browser_process->local_state();
  ScopedDictPrefUpdate shared_extensions(local_state, kSharedExtensions);
  std::string* shared_path = nullptr;
  base::Value::List* users = nullptr;
  if (base::Value::Dict* extension_info = shared_extensions->FindDict(id)) {
    if (base::Value::Dict* version_info = extension_info->FindDict(version)) {
      shared_path = version_info->FindString(kSharedExtensionPath);
      users = version_info->FindList(kSharedExtensionUsers);
    }
  }

  if (shared_path && users) {
    // This extension version already in shared location.
    bool user_found = false;
    for (const base::Value& user : *users) {
      const std::string* temp = user.GetIfString();
      if (temp && *temp == user_id) {
        // Re-installation for the same user.
        user_found = true;
        break;
      }
    }
    if (!user_found)
      users->Append(user_id);

    // unpacked_extension_root will be deleted by CrxInstaller.
    GetExtensionFileTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), base::FilePath(*shared_path)));
  } else {
    // Desired version is not found in shared location.
    ExtensionAssetsManagerHelper* helper =
        ExtensionAssetsManagerHelper::GetInstance();
    if (helper->RecordSharedInstall(id, version, unpacked_extension_root,
                                    local_install_dir, profile,
                                    std::move(callback))) {
      // There is no install in progress for given <id, version> so run install.
      GetExtensionFileTaskRunner()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &ExtensionAssetsManagerChromeOS::InstallSharedExtension, id,
              version, unpacked_extension_root));
    }
  }
}

// static
void ExtensionAssetsManagerChromeOS::InstallSharedExtension(
      const std::string& id,
      const std::string& version,
      const base::FilePath& unpacked_extension_root) {
  base::FilePath shared_install_dir = GetSharedInstallDir();
  base::FilePath shared_version_dir = file_util::InstallExtension(
      unpacked_extension_root, id, version, shared_install_dir);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ExtensionAssetsManagerChromeOS::InstallSharedExtensionDone, id,
          version, shared_version_dir));
}

// static
void ExtensionAssetsManagerChromeOS::InstallSharedExtensionDone(
    const std::string& id,
    const std::string& version,
    const base::FilePath& shared_version_dir) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ExtensionAssetsManagerHelper* helper =
      ExtensionAssetsManagerHelper::GetInstance();
  ExtensionAssetsManagerHelper::PendingInstallList pending_installs;
  helper->SharedInstallDone(id, version, &pending_installs);

  if (shared_version_dir.empty()) {
    // Installation to shared location failed, try local dir.
    // TODO(dpolukhin): add UMA stats reporting.
    for (size_t i = 0; i < pending_installs.size(); i++) {
      ExtensionAssetsManagerHelper::PendingInstallInfo& info =
          pending_installs[i];
      GetExtensionFileTaskRunner()->PostTask(
          FROM_HERE,
          base::BindOnce(&ExtensionAssetsManagerChromeOS::InstallLocalExtension,
                         id, version, info.unpacked_extension_root,
                         info.local_install_dir, std::move(info.callback)));
    }
    return;
  }

  PrefService* local_state = g_browser_process->local_state();
  ScopedDictPrefUpdate shared_extensions(local_state, kSharedExtensions);
  base::Value::Dict* extension_info_weak = shared_extensions->EnsureDict(id);

  CHECK(!shared_extensions->Find(version));
  base::Value::Dict version_info;
  version_info.Set(kSharedExtensionPath, shared_version_dir.value());

  base::Value::List users;
  for (size_t i = 0; i < pending_installs.size(); i++) {
    ExtensionAssetsManagerHelper::PendingInstallInfo& info =
        pending_installs[i];
    users.Append(info.profile->GetProfileUserName());

    GetExtensionFileTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(info.callback), shared_version_dir));
  }
  version_info.Set(kSharedExtensionUsers, std::move(users));
  extension_info_weak->Set(version, std::move(version_info));
}

// static
void ExtensionAssetsManagerChromeOS::InstallLocalExtension(
    const std::string& id,
    const std::string& version,
    const base::FilePath& unpacked_extension_root,
    const base::FilePath& local_install_dir,
    InstallExtensionCallback callback) {
  std::move(callback).Run(file_util::InstallExtension(
      unpacked_extension_root, id, version, local_install_dir));
}

// static
void ExtensionAssetsManagerChromeOS::MarkSharedExtensionUnused(
    const std::string& id,
    const std::string& profile_user_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  PrefService* local_state = g_browser_process->local_state();
  ScopedDictPrefUpdate shared_extensions(local_state, kSharedExtensions);
  base::Value::Dict& shared_extensions_dict = shared_extensions.Get();
  base::Value::Dict* extension_info = shared_extensions_dict.FindDict(id);
  if (!extension_info) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  std::vector<std::string> versions;
  versions.reserve(extension_info->size());
  for (const auto kv : *extension_info) {
    versions.push_back(kv.first);
  }

  base::Value user_name(profile_user_name);
  for (std::vector<std::string>::const_iterator it = versions.begin();
       it != versions.end(); it++) {
    base::Value::Dict* version_info = extension_info->FindDict(*it);
    if (!version_info) {
      NOTREACHED_IN_MIGRATION();
      continue;
    }
    base::Value::List* users = version_info->FindList(kSharedExtensionUsers);
    if (!users) {
      NOTREACHED_IN_MIGRATION();
      continue;
    }
    if (users->EraseValue(user_name) && users->empty()) {
      std::string* shared_path = version_info->FindString(kSharedExtensionPath);
      if (!shared_path) {
        NOTREACHED_IN_MIGRATION();
        continue;
      }
      GetExtensionFileTaskRunner()->PostTask(
          FROM_HERE,
          base::BindOnce(&ExtensionAssetsManagerChromeOS::DeleteSharedVersion,
                         base::FilePath(*shared_path)));
      extension_info->Remove(*it);
    }
  }
  if (extension_info->empty()) {
    shared_extensions_dict.Remove(id);
    // Don't remove extension dir in shared location. It will be removed by GC
    // when it is safe to do so, and this avoids a race condition between
    // concurrent uninstall by one user and install by another.
  }
}

// static
void ExtensionAssetsManagerChromeOS::DeleteSharedVersion(
    const base::FilePath& shared_version_dir) {
  CHECK(GetSharedInstallDir().IsParent(shared_version_dir));
  base::DeletePathRecursively(shared_version_dir);
}

// static
bool ExtensionAssetsManagerChromeOS::CleanUpExtension(
    const std::string& id,
    base::Value::Dict& extension_info,
    std::multimap<std::string, base::FilePath>* live_extension_paths) {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (!user_manager) {
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  std::vector<std::string> versions;
  versions.reserve(extension_info.size());
  for (const auto it : extension_info) {
    versions.push_back(it.first);
  }

  for (std::vector<std::string>::const_iterator it = versions.begin();
       it != versions.end(); it++) {
    base::Value::Dict* version_info = extension_info.FindDict(*it);
    if (!version_info) {
      NOTREACHED_IN_MIGRATION();
      return false;
    }
    base::Value::List* users_list =
        version_info->FindList(kSharedExtensionUsers);
    const std::string* shared_path =
        version_info->FindString(kSharedExtensionPath);
    if (!users_list || !shared_path) {
      NOTREACHED_IN_MIGRATION();
      return false;
    }

    for (auto iter = users_list->begin(); iter != users_list->end();) {
      const std::string* user_id = iter->GetIfString();
      if (!user_id) {
        NOTREACHED_IN_MIGRATION();
        return false;
      }
      const user_manager::User* user =
          user_manager->FindUser(AccountId::FromUserEmail(*user_id));
      bool not_used = false;
      if (!user) {
        not_used = true;
      } else if (user->is_logged_in()) {
        // For logged in user also check that this path is actually used as
        // installed extension or as delayed install.
        Profile* profile = ash::ProfileHelper::Get()->GetProfileByUser(user);
        DCHECK(profile);
        ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile);
        if (!extension_prefs || extension_prefs->pref_service()->ReadOnly())
          return false;

        std::optional<ExtensionInfo> info =
            extension_prefs->GetInstalledExtensionInfo(id);
        if (!info || info->extension_path != base::FilePath(*shared_path)) {
          info = extension_prefs->GetDelayedInstallInfo(id);
          if (!info || info->extension_path != base::FilePath(*shared_path)) {
            not_used = true;
          }
        }
      }

      if (not_used) {
        iter = users_list->erase(iter);
      } else {
        ++iter;
      }
    }

    if (users_list->empty()) {
      extension_info.Remove(*it);
    } else {
      live_extension_paths->insert(
          std::make_pair(id, base::FilePath(*shared_path)));
    }
  }

  return true;
}

}  // namespace extensions
