// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_assets_manager_chromeos.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/sequenced_task_runner.h"
#include "base/sys_info.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/chromeos_switches.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_url_handlers.h"

using content::BrowserThread;

namespace extensions {
namespace {

// Path to shared extensions install dir.
const char kSharedExtensionsDir[] = "/var/cache/shared_extensions";

// Shared install dir overrider for tests only.
static const base::FilePath* g_shared_install_dir_override = NULL;

// This helper class lives on UI thread only. Main purpose of this class is to
// track shared installation in progress between multiple profiles.
class ExtensionAssetsManagerHelper {
 public:
  // Info about pending install request.
  struct PendingInstallInfo {
    base::FilePath unpacked_extension_root;
    base::FilePath local_install_dir;
    Profile* profile;
    ExtensionAssetsManager::InstallExtensionCallback callback;
  };
  typedef std::vector<PendingInstallInfo> PendingInstallList;

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
    install_info.callback = callback;

    std::vector<PendingInstallInfo>& callbacks =
        install_queue_[InstallQueue::key_type(id, version)];
    callbacks.push_back(install_info);

    return callbacks.size() == 1;
  }

  // Remove record about shared installation in progress and return
  // |pending_installs|.
  void SharedInstallDone(const std::string& id,
                         const std::string& version,
                         PendingInstallList* pending_installs) {
    InstallQueue::iterator it = install_queue_.find(
        InstallQueue::key_type(id, version));
    DCHECK(it != install_queue_.end());
    pending_installs->swap(it->second);
    install_queue_.erase(it);
  }

 private:
  friend struct base::DefaultSingletonTraits<ExtensionAssetsManagerHelper>;

  ExtensionAssetsManagerHelper() {}
  ~ExtensionAssetsManagerHelper() {}

  // Extension ID + version pair.
  typedef std::pair<std::string, std::string> InstallItem;

  // Queue of pending installs in progress.
  typedef std::map<InstallItem, std::vector<PendingInstallInfo> > InstallQueue;

  InstallQueue install_queue_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionAssetsManagerHelper);
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
    g_shared_install_dir_override = NULL;
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
    InstallExtensionCallback callback) {
  if (!CanShareAssets(extension, unpacked_extension_root)) {
    InstallLocalExtension(extension->id(),
                          extension->VersionString(),
                          unpacked_extension_root,
                          local_install_dir,
                          callback);
    return;
  }

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::Bind(&ExtensionAssetsManagerChromeOS::CheckSharedExtension,
                 extension->id(), extension->VersionString(),
                 unpacked_extension_root, local_install_dir, profile,
                 callback));
}

void ExtensionAssetsManagerChromeOS::UninstallExtension(
    const std::string& id,
    Profile* profile,
    const base::FilePath& local_install_dir,
    const base::FilePath& extension_root) {
  if (local_install_dir.IsParent(extension_root)) {
    file_util::UninstallExtension(local_install_dir, id);
    return;
  }

  if (GetSharedInstallDir().IsParent(extension_root)) {
    // In some test extensions installed outside local_install_dir emulate
    // previous behavior that just do nothing in this case.
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::Bind(&ExtensionAssetsManagerChromeOS::MarkSharedExtensionUnused,
                   id, profile));
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

  DictionaryPrefUpdate shared_extensions(local_state, kSharedExtensions);
  std::vector<std::string> extensions;
  extensions.reserve(shared_extensions->size());
  for (base::DictionaryValue::Iterator it(*shared_extensions);
       !it.IsAtEnd(); it.Advance()) {
    extensions.push_back(it.key());
  }

  for (std::vector<std::string>::iterator it = extensions.begin();
       it != extensions.end(); it++) {
    base::DictionaryValue* extension_info = NULL;
    if (!shared_extensions->GetDictionary(*it, &extension_info)) {
      NOTREACHED();
      return false;
    }
    if (!CleanUpExtension(*it, extension_info, live_extension_paths)) {
      return false;
    }
    if (extension_info->empty())
      shared_extensions->RemoveWithoutPathExpansion(*it, NULL);
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
    const base::FilePath& unpacked_extension_root) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kEnableExtensionAssetsSharing)) {
    return false;
  }

  GURL update_url = ManifestURL::GetUpdateURL(extension);
  if (!update_url.is_empty() &&
      !extension_urls::IsWebstoreUpdateUrl(update_url)) {
    return false;
  }

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
    NOTREACHED();
    return;
  }

  if (user_manager->IsUserNonCryptohomeDataEphemeral(
          AccountId::FromUserEmail(user_id)) ||
      !user_manager->IsLoggedInAsUserWithGaiaAccount()) {
    // Don't cache anything in shared location for ephemeral user or special
    // user types.
    GetExtensionFileTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&ExtensionAssetsManagerChromeOS::InstallLocalExtension, id,
                   version, unpacked_extension_root, local_install_dir,
                   callback));
    return;
  }

  PrefService* local_state = g_browser_process->local_state();
  DictionaryPrefUpdate shared_extensions(local_state, kSharedExtensions);
  base::DictionaryValue* extension_info = NULL;
  base::DictionaryValue* version_info = NULL;
  base::ListValue* users = NULL;
  std::string shared_path;
  if (shared_extensions->GetDictionary(id, &extension_info) &&
      extension_info->GetDictionaryWithoutPathExpansion(
          version, &version_info) &&
      version_info->GetString(kSharedExtensionPath, &shared_path) &&
      version_info->GetList(kSharedExtensionUsers, &users)) {
    // This extension version already in shared location.
    size_t users_size = users->GetSize();
    bool user_found = false;
    for (size_t i = 0; i < users_size; i++) {
      std::string temp;
      if (users->GetString(i, &temp) && temp == user_id) {
        // Re-installation for the same user.
        user_found = true;
        break;
      }
    }
    if (!user_found)
      users->AppendString(user_id);

    // unpacked_extension_root will be deleted by CrxInstaller.
    GetExtensionFileTaskRunner()->PostTask(
        FROM_HERE, base::Bind(callback, base::FilePath(shared_path)));
  } else {
    // Desired version is not found in shared location.
    ExtensionAssetsManagerHelper* helper =
        ExtensionAssetsManagerHelper::GetInstance();
    if (helper->RecordSharedInstall(id, version, unpacked_extension_root,
                                    local_install_dir, profile, callback)) {
      // There is no install in progress for given <id, version> so run install.
      GetExtensionFileTaskRunner()->PostTask(
          FROM_HERE,
          base::Bind(&ExtensionAssetsManagerChromeOS::InstallSharedExtension,
                     id, version, unpacked_extension_root));
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
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::Bind(&ExtensionAssetsManagerChromeOS::InstallSharedExtensionDone,
                 id, version, shared_version_dir));
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
          base::Bind(&ExtensionAssetsManagerChromeOS::InstallLocalExtension, id,
                     version, info.unpacked_extension_root,
                     info.local_install_dir, info.callback));
    }
    return;
  }

  PrefService* local_state = g_browser_process->local_state();
  DictionaryPrefUpdate shared_extensions(local_state, kSharedExtensions);
  base::DictionaryValue* extension_info_weak = NULL;
  if (!shared_extensions->GetDictionary(id, &extension_info_weak)) {
    auto extension_info = std::make_unique<base::DictionaryValue>();
    extension_info_weak = extension_info.get();
    shared_extensions->Set(id, std::move(extension_info));
  }

  CHECK(!shared_extensions->HasKey(version));
  auto version_info = std::make_unique<base::DictionaryValue>();
  version_info->SetString(kSharedExtensionPath, shared_version_dir.value());

  auto users = std::make_unique<base::ListValue>();
  for (size_t i = 0; i < pending_installs.size(); i++) {
    ExtensionAssetsManagerHelper::PendingInstallInfo& info =
        pending_installs[i];
      users->AppendString(info.profile->GetProfileUserName());

      GetExtensionFileTaskRunner()->PostTask(
          FROM_HERE, base::Bind(info.callback, shared_version_dir));
  }
  version_info->Set(kSharedExtensionUsers, std::move(users));
  extension_info_weak->SetWithoutPathExpansion(version,
                                               std::move(version_info));
}

// static
void ExtensionAssetsManagerChromeOS::InstallLocalExtension(
    const std::string& id,
    const std::string& version,
    const base::FilePath& unpacked_extension_root,
    const base::FilePath& local_install_dir,
    InstallExtensionCallback callback) {
  callback.Run(file_util::InstallExtension(
      unpacked_extension_root, id, version, local_install_dir));
}

// static
void ExtensionAssetsManagerChromeOS::MarkSharedExtensionUnused(
    const std::string& id,
    Profile* profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  PrefService* local_state = g_browser_process->local_state();
  DictionaryPrefUpdate shared_extensions(local_state, kSharedExtensions);
  base::DictionaryValue* extension_info = NULL;
  if (!shared_extensions->GetDictionary(id, &extension_info)) {
    NOTREACHED();
    return;
  }

  std::vector<std::string> versions;
  versions.reserve(extension_info->size());
  for (base::DictionaryValue::Iterator it(*extension_info);
       !it.IsAtEnd();
       it.Advance()) {
    versions.push_back(it.key());
  }

  base::Value user_name(profile->GetProfileUserName());
  for (std::vector<std::string>::const_iterator it = versions.begin();
       it != versions.end(); it++) {
    base::DictionaryValue* version_info = NULL;
    if (!extension_info->GetDictionaryWithoutPathExpansion(*it,
                                                           &version_info)) {
      NOTREACHED();
      continue;
    }
    base::ListValue* users = NULL;
    if (!version_info->GetList(kSharedExtensionUsers, &users)) {
      NOTREACHED();
      continue;
    }
    if (users->Remove(user_name, NULL) && !users->GetSize()) {
      std::string shared_path;
      if (!version_info->GetString(kSharedExtensionPath, &shared_path)) {
        NOTREACHED();
        continue;
      }
      GetExtensionFileTaskRunner()->PostTask(
          FROM_HERE,
          base::Bind(&ExtensionAssetsManagerChromeOS::DeleteSharedVersion,
                     base::FilePath(shared_path)));
      extension_info->RemoveWithoutPathExpansion(*it, NULL);
    }
  }
  if (extension_info->empty()) {
    shared_extensions->RemoveWithoutPathExpansion(id, NULL);
    // Don't remove extension dir in shared location. It will be removed by GC
    // when it is safe to do so, and this avoids a race condition between
    // concurrent uninstall by one user and install by another.
  }
}

// static
void ExtensionAssetsManagerChromeOS::DeleteSharedVersion(
    const base::FilePath& shared_version_dir) {
  CHECK(GetSharedInstallDir().IsParent(shared_version_dir));
  base::DeleteFile(shared_version_dir, true);  // recursive.
}

// static
bool ExtensionAssetsManagerChromeOS::CleanUpExtension(
    const std::string& id,
    base::DictionaryValue* extension_info,
    std::multimap<std::string, base::FilePath>* live_extension_paths) {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (!user_manager) {
    NOTREACHED();
    return false;
  }

  std::vector<std::string> versions;
  versions.reserve(extension_info->size());
  for (base::DictionaryValue::Iterator it(*extension_info);
       !it.IsAtEnd(); it.Advance()) {
    versions.push_back(it.key());
  }

  for (std::vector<std::string>::const_iterator it = versions.begin();
       it != versions.end(); it++) {
    base::DictionaryValue* version_info = NULL;
    base::ListValue* users = NULL;
    std::string shared_path;
    if (!extension_info->GetDictionaryWithoutPathExpansion(*it,
                                                           &version_info) ||
        !version_info->GetList(kSharedExtensionUsers, &users) ||
        !version_info->GetString(kSharedExtensionPath, &shared_path)) {
      NOTREACHED();
      return false;
    }

    size_t num_users = users->GetSize();
    for (size_t i = 0; i < num_users; i++) {
      std::string user_id;
      if (!users->GetString(i, &user_id)) {
        NOTREACHED();
        return false;
      }
      const user_manager::User* user =
          user_manager->FindUser(AccountId::FromUserEmail(user_id));
      bool not_used = false;
      if (!user) {
        not_used = true;
      } else if (user->is_logged_in()) {
        // For logged in user also check that this path is actually used as
        // installed extension or as delayed install.
        Profile* profile =
            chromeos::ProfileHelper::Get()->GetProfileByUserUnsafe(user);
        ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile);
        if (!extension_prefs || extension_prefs->pref_service()->ReadOnly())
          return false;

        std::unique_ptr<ExtensionInfo> info =
            extension_prefs->GetInstalledExtensionInfo(id);
        if (!info || info->extension_path != base::FilePath(shared_path)) {
          info = extension_prefs->GetDelayedInstallInfo(id);
          if (!info || info->extension_path != base::FilePath(shared_path)) {
            not_used = true;
          }
        }
      }

      if (not_used) {
        users->Remove(i, NULL);

        i--;
        num_users--;
      }
    }

    if (num_users) {
      live_extension_paths->insert(
          std::make_pair(id, base::FilePath(shared_path)));
    } else {
      extension_info->RemoveWithoutPathExpansion(*it, NULL);
    }
  }

  return true;
}

}  // namespace extensions
