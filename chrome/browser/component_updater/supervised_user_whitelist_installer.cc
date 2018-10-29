// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/supervised_user_whitelist_installer.h"

#include <stddef.h>

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/path_service.h"
#include "base/scoped_observer.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/supervised_user/supervised_user_whitelist_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "services/data_decoder/public/cpp/json_sanitizer.h"

namespace component_updater {

namespace {

const char kSanitizedWhitelistExtension[] = ".json";

const char kWhitelistedContent[] = "whitelisted_content";
const char kSites[] = "sites";

const char kClients[] = "clients";
const char kName[] = "name";

// These are copies of extensions::manifest_keys::kName and kShortName. They
// are duplicated here because we mustn't depend on code from extensions/
// (since it's not built on Android).
const char kExtensionName[] = "name";
const char kExtensionShortName[] = "short_name";
const char kExtensionIcons[] = "icons";
const char kExtensionLargeIcon[] = "128";

constexpr base::TaskTraits kTaskTraits = {
    base::MayBlock(), base::TaskPriority::BEST_EFFORT,
    base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

base::string16 GetWhitelistTitle(const base::DictionaryValue& manifest) {
  base::string16 title;
  if (!manifest.GetString(kExtensionShortName, &title))
    manifest.GetString(kExtensionName, &title);
  return title;
}

base::FilePath GetSafeFilePath(const base::DictionaryValue& dictionary,
                               const std::string& key,
                               const base::FilePath& install_dir) {
  const base::Value* path_value = nullptr;
  if (!dictionary.Get(key, &path_value))
    return base::FilePath();
  base::FilePath path;
  if (!base::GetValueAsFilePath(*path_value, &path))
    return base::FilePath();
  // Path components ("..") are not allowed.
  if (path.ReferencesParent())
    return base::FilePath();

  return install_dir.Append(path);
}

base::FilePath GetLargeIconPath(const base::DictionaryValue& manifest,
                                const base::FilePath& install_dir) {
  const base::DictionaryValue* icons = nullptr;
  if (!manifest.GetDictionary(kExtensionIcons, &icons))
    return base::FilePath();

  return GetSafeFilePath(*icons, kExtensionLargeIcon, install_dir);
}

base::FilePath GetRawWhitelistPath(const base::DictionaryValue& manifest,
                                   const base::FilePath& install_dir) {
  const base::DictionaryValue* whitelist_dict = nullptr;
  if (!manifest.GetDictionary(kWhitelistedContent, &whitelist_dict))
    return base::FilePath();

  return GetSafeFilePath(*whitelist_dict, kSites, install_dir);
}

base::FilePath GetSanitizedWhitelistPath(const std::string& crx_id) {
  base::FilePath base_dir;
  base::PathService::Get(chrome::DIR_SUPERVISED_USER_INSTALLED_WHITELISTS,
                         &base_dir);
  return base_dir.empty()
             ? base::FilePath()
             : base_dir.AppendASCII(crx_id + kSanitizedWhitelistExtension);
}

void RecordUncleanUninstall() {
  base::CreateSingleThreadTaskRunnerWithTraits({content::BrowserThread::UI})
      ->PostTask(
          FROM_HERE,
          base::BindOnce(&base::RecordAction,
                         base::UserMetricsAction(
                             "ManagedUsers_Whitelist_UncleanUninstall")));
}

void OnWhitelistSanitizationError(const base::FilePath& whitelist,
                                  const std::string& error) {
  LOG(WARNING) << "Invalid whitelist " << whitelist.value() << ": " << error;
}

void DeleteFileOnTaskRunner(const base::FilePath& path) {
  if (!base::DeleteFile(path, true))
    DPLOG(ERROR) << "Couldn't delete " << path.value();
}

void OnWhitelistSanitizationResult(
    const std::string& crx_id,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::OnceClosure callback,
    const std::string& result) {
  const base::FilePath sanitized_whitelist_path =
      GetSanitizedWhitelistPath(crx_id);
  const base::FilePath install_directory = sanitized_whitelist_path.DirName();
  if (!base::DirectoryExists(install_directory)) {
    if (!base::CreateDirectory(install_directory)) {
      PLOG(ERROR) << "Could't create directory " << install_directory.value();
      return;
    }
  }

  const int size = result.size();
  if (base::WriteFile(sanitized_whitelist_path, result.data(), size) != size) {
    PLOG(ERROR) << "Couldn't write file " << sanitized_whitelist_path.value();
    return;
  }
  task_runner->PostTask(FROM_HERE, std::move(callback));
}

void CheckForSanitizedWhitelistOnTaskRunner(
    const std::string& crx_id,
    const base::FilePath& whitelist_path,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::Closure& callback) {
  if (base::PathExists(GetSanitizedWhitelistPath(crx_id))) {
    task_runner->PostTask(FROM_HERE, callback);
    return;
  }

  std::string unsafe_json;
  if (!base::ReadFileToString(whitelist_path, &unsafe_json)) {
    PLOG(ERROR) << "Couldn't read file " << whitelist_path.value();
    return;
  }

  data_decoder::JsonSanitizer::Sanitize(
      content::ServiceManagerConnection::GetForProcess()->GetConnector(),
      unsafe_json,
      base::Bind(&OnWhitelistSanitizationResult, crx_id, task_runner, callback),
      base::Bind(&OnWhitelistSanitizationError, whitelist_path));
}

void RemoveUnregisteredWhitelistsOnTaskRunner(
    const std::set<std::string>& registered_whitelists) {
  base::FilePath base_dir;
  base::PathService::Get(DIR_SUPERVISED_USER_WHITELISTS, &base_dir);
  if (!base_dir.empty()) {
    base::FileEnumerator file_enumerator(base_dir, false,
                                         base::FileEnumerator::DIRECTORIES);
    for (base::FilePath path = file_enumerator.Next(); !path.value().empty();
         path = file_enumerator.Next()) {
      const std::string crx_id = path.BaseName().MaybeAsASCII();

      // Ignore folders that don't have valid CRX ID names. These folders are
      // not managed by the component installer, so do not try to remove them.
      if (!crx_file::id_util::IdIsValid(crx_id))
        continue;

      // Ignore folders that correspond to registered whitelists.
      if (base::ContainsKey(registered_whitelists, crx_id))
        continue;

      RecordUncleanUninstall();

      DeleteFileOnTaskRunner(path);
    }
  }

  base::PathService::Get(chrome::DIR_SUPERVISED_USER_INSTALLED_WHITELISTS,
                         &base_dir);
  if (!base_dir.empty()) {
    base::FilePath pattern(FILE_PATH_LITERAL("*"));
    pattern = pattern.AppendASCII(kSanitizedWhitelistExtension);
    base::FileEnumerator file_enumerator(
        base_dir, false, base::FileEnumerator::FILES, pattern.value());
    for (base::FilePath path = file_enumerator.Next(); !path.value().empty();
         path = file_enumerator.Next()) {
      // Ignore files that don't have valid CRX ID names. These files are not
      // managed by the component installer, so do not try to remove them.
      const std::string filename = path.BaseName().MaybeAsASCII();
      DCHECK(base::EndsWith(filename, kSanitizedWhitelistExtension,
                            base::CompareCase::SENSITIVE));

      const std::string crx_id = filename.substr(
          filename.size() - strlen(kSanitizedWhitelistExtension));

      if (!crx_file::id_util::IdIsValid(crx_id))
        continue;

      // Ignore files that correspond to registered whitelists.
      if (base::ContainsKey(registered_whitelists, crx_id))
        continue;

      RecordUncleanUninstall();

      DeleteFileOnTaskRunner(path);
    }
  }
}

class SupervisedUserWhitelistComponentInstallerPolicy
    : public ComponentInstallerPolicy {
 public:
  using RawWhitelistReadyCallback =
      base::Callback<void(const base::string16&, /* title */
                          const base::FilePath&, /* icon_path */
                          const base::FilePath& /* whitelist_path */)>;

  SupervisedUserWhitelistComponentInstallerPolicy(
      const std::string& crx_id,
      const std::string& name,
      const RawWhitelistReadyCallback& callback)
      : crx_id_(crx_id), name_(name), callback_(callback) {}
  ~SupervisedUserWhitelistComponentInstallerPolicy() override {}

 private:
  // ComponentInstallerPolicy overrides:
  bool VerifyInstallation(const base::DictionaryValue& manifest,
                          const base::FilePath& install_dir) const override;
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::DictionaryValue& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      std::unique_ptr<base::DictionaryValue> manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;
  std::vector<std::string> GetMimeTypes() const override;

  std::string crx_id_;
  std::string name_;
  RawWhitelistReadyCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserWhitelistComponentInstallerPolicy);
};

bool SupervisedUserWhitelistComponentInstallerPolicy::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  // Check whether the whitelist exists at the path specified by the manifest.
  // This does not check whether the whitelist is wellformed.
  return base::PathExists(GetRawWhitelistPath(manifest, install_dir));
}

bool SupervisedUserWhitelistComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return false;
}

bool SupervisedUserWhitelistComponentInstallerPolicy::
    RequiresNetworkEncryption() const {
  return true;
}

update_client::CrxInstaller::Result
SupervisedUserWhitelistComponentInstallerPolicy::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  // Delete the existing sanitized whitelist.
  const bool success =
      base::DeleteFile(GetSanitizedWhitelistPath(crx_id_), false);
  return update_client::CrxInstaller::Result(
      success ? update_client::InstallError::NONE
              : update_client::InstallError::GENERIC_ERROR);
}

void SupervisedUserWhitelistComponentInstallerPolicy::OnCustomUninstall() {}

void SupervisedUserWhitelistComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  // TODO(treib): Before getting the title, we should localize the manifest
  // using extension_l10n_util::LocalizeExtension, but that doesn't exist on
  // Android. crbug.com/558387
  callback_.Run(GetWhitelistTitle(*manifest),
                GetLargeIconPath(*manifest, install_dir),
                GetRawWhitelistPath(*manifest, install_dir));
}

base::FilePath
SupervisedUserWhitelistComponentInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(component_updater::kSupervisedUserWhitelistDirName)
      .AppendASCII(crx_id_);
}

void SupervisedUserWhitelistComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  *hash = SupervisedUserWhitelistInstaller::GetHashFromCrxId(crx_id_);
}

std::string SupervisedUserWhitelistComponentInstallerPolicy::GetName() const {
  return name_;
}

update_client::InstallerAttributes
SupervisedUserWhitelistComponentInstallerPolicy::GetInstallerAttributes()
    const {
  return update_client::InstallerAttributes();
}

std::vector<std::string>
SupervisedUserWhitelistComponentInstallerPolicy::GetMimeTypes() const {
  return std::vector<std::string>();
}

class SupervisedUserWhitelistInstallerImpl
    : public SupervisedUserWhitelistInstaller,
      public ProfileAttributesStorage::Observer {
 public:
  SupervisedUserWhitelistInstallerImpl(
      ComponentUpdateService* cus,
      ProfileAttributesStorage* profile_attributes_storage,
      PrefService* local_state);
  ~SupervisedUserWhitelistInstallerImpl() override {}

 private:
  void RegisterComponent(const std::string& crx_id,
                         const std::string& name,
                         base::OnceClosure callback);
  void RegisterNewComponent(const std::string& crx_id, const std::string& name);
  bool UnregisterWhitelistInternal(base::DictionaryValue* pref_dict,
                                   const std::string& client_id,
                                   const std::string& crx_id);

  void OnRawWhitelistReady(const std::string& crx_id,
                           const base::string16& title,
                           const base::FilePath& large_icon_path,
                           const base::FilePath& whitelist_path);
  void OnSanitizedWhitelistReady(const std::string& crx_id,
                                 const base::string16& title,
                                 const base::FilePath& large_icon_path);

  // SupervisedUserWhitelistInstaller overrides:
  void RegisterComponents() override;
  void Subscribe(const WhitelistReadyCallback& callback) override;
  void RegisterWhitelist(const std::string& client_id,
                         const std::string& crx_id,
                         const std::string& name) override;
  void UnregisterWhitelist(const std::string& client_id,
                           const std::string& crx_id) override;

  // ProfileAttributesStorage::Observer overrides:
  void OnProfileWillBeRemoved(const base::FilePath& profile_path) override;

  ComponentUpdateService* cus_ = nullptr;
  PrefService* local_state_ = nullptr;

  std::vector<WhitelistReadyCallback> callbacks_;

  ScopedObserver<ProfileAttributesStorage, ProfileAttributesStorage::Observer>
      observer_;

  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_ =
      base::CreateSequencedTaskRunnerWithTraits(kTaskTraits);

  base::WeakPtrFactory<SupervisedUserWhitelistInstallerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserWhitelistInstallerImpl);
};

SupervisedUserWhitelistInstallerImpl::SupervisedUserWhitelistInstallerImpl(
    ComponentUpdateService* cus,
    ProfileAttributesStorage* profile_attributes_storage,
    PrefService* local_state)
    : cus_(cus),
      local_state_(local_state),
      observer_(this),
      weak_ptr_factory_(this) {
  DCHECK(cus);
  DCHECK(local_state);
  observer_.Add(profile_attributes_storage);
}

void SupervisedUserWhitelistInstallerImpl::RegisterComponent(
    const std::string& crx_id,
    const std::string& name,
    base::OnceClosure callback) {
  std::unique_ptr<ComponentInstallerPolicy> policy =
      std::make_unique<SupervisedUserWhitelistComponentInstallerPolicy>(
          crx_id, name,
          base::Bind(&SupervisedUserWhitelistInstallerImpl::OnRawWhitelistReady,
                     weak_ptr_factory_.GetWeakPtr(), crx_id));
  scoped_refptr<ComponentInstaller> installer =
      base::MakeRefCounted<ComponentInstaller>(std::move(policy));
  installer->Register(cus_, std::move(callback));
}

void SupervisedUserWhitelistInstallerImpl::RegisterNewComponent(
    const std::string& crx_id,
    const std::string& name) {
  RegisterComponent(
      crx_id, name,
      base::BindOnce(&SupervisedUserWhitelistInstaller::TriggerComponentUpdate,
                     &cus_->GetOnDemandUpdater(), crx_id));
}

bool SupervisedUserWhitelistInstallerImpl::UnregisterWhitelistInternal(
    base::DictionaryValue* pref_dict,
    const std::string& client_id,
    const std::string& crx_id) {
  base::DictionaryValue* whitelist_dict = nullptr;
  bool success =
      pref_dict->GetDictionaryWithoutPathExpansion(crx_id, &whitelist_dict);
  DCHECK(success);
  base::ListValue* clients = nullptr;
  success = whitelist_dict->GetList(kClients, &clients);

  const bool removed = clients->Remove(base::Value(client_id), nullptr);

  if (!clients->empty())
    return removed;

  pref_dict->RemoveWithoutPathExpansion(crx_id, nullptr);
  const bool result = cus_->UnregisterComponent(crx_id);
  DCHECK(result);

  sequenced_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DeleteFileOnTaskRunner,
                                GetSanitizedWhitelistPath(crx_id)));
  return removed;
}

void SupervisedUserWhitelistInstallerImpl::OnRawWhitelistReady(
    const std::string& crx_id,
    const base::string16& title,
    const base::FilePath& large_icon_path,
    const base::FilePath& whitelist_path) {
  // TODO(sorin): avoid using a single thread task runner crbug.com/744718.
  auto task_runner = base::CreateSingleThreadTaskRunnerWithTraits(
      kTaskTraits, base::SingleThreadTaskRunnerThreadMode::SHARED);
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CheckForSanitizedWhitelistOnTaskRunner, crx_id, whitelist_path,
          base::ThreadTaskRunnerHandle::Get(),
          base::Bind(
              &SupervisedUserWhitelistInstallerImpl::OnSanitizedWhitelistReady,
              weak_ptr_factory_.GetWeakPtr(), crx_id, title, large_icon_path)));
}

void SupervisedUserWhitelistInstallerImpl::OnSanitizedWhitelistReady(
    const std::string& crx_id,
    const base::string16& title,
    const base::FilePath& large_icon_path) {
  for (const WhitelistReadyCallback& callback : callbacks_)
    callback.Run(crx_id, title, large_icon_path,
                 GetSanitizedWhitelistPath(crx_id));
}

void SupervisedUserWhitelistInstallerImpl::RegisterComponents() {
  const std::map<std::string, std::string> command_line_whitelists =
      SupervisedUserWhitelistService::GetWhitelistsFromCommandLine();

  std::set<std::string> registered_whitelists;
  std::set<std::string> stale_whitelists;
  DictionaryPrefUpdate update(local_state_,
                              prefs::kRegisteredSupervisedUserWhitelists);
  base::DictionaryValue* whitelists = update.Get();
  for (base::DictionaryValue::Iterator it(*whitelists); !it.IsAtEnd();
       it.Advance()) {
    const base::DictionaryValue* dict = nullptr;
    it.value().GetAsDictionary(&dict);

    const std::string& id = it.key();

    // Skip whitelists with no clients. This can happen when a whitelist was
    // previously registered on the command line but isn't anymore.
    const base::ListValue* clients = nullptr;
    if ((!dict->GetList(kClients, &clients) || clients->empty()) &&
        !base::ContainsKey(command_line_whitelists, id)) {
      stale_whitelists.insert(id);
      continue;
    }

    std::string name;
    const bool result = dict->GetString(kName, &name);
    DCHECK(result);
    RegisterComponent(id, name, base::OnceClosure());

    registered_whitelists.insert(id);
  }

  // Clean up stale whitelists as determined above.
  for (const std::string& id : stale_whitelists)
    whitelists->RemoveWithoutPathExpansion(id, nullptr);

  sequenced_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&RemoveUnregisteredWhitelistsOnTaskRunner,
                                registered_whitelists));
}

void SupervisedUserWhitelistInstallerImpl::Subscribe(
    const WhitelistReadyCallback& callback) {
  return callbacks_.push_back(callback);
}

void SupervisedUserWhitelistInstallerImpl::RegisterWhitelist(
    const std::string& client_id,
    const std::string& crx_id,
    const std::string& name) {
  DictionaryPrefUpdate update(local_state_,
                              prefs::kRegisteredSupervisedUserWhitelists);
  base::Value* pref_dict = update.Get();
  base::Value* whitelist_dict =
      pref_dict->FindKeyOfType(crx_id, base::Value::Type::DICTIONARY);
  const bool newly_added = !whitelist_dict;
  if (newly_added) {
    whitelist_dict =
        pref_dict->SetKey(crx_id, base::Value(base::Value::Type::DICTIONARY));
    whitelist_dict->SetKey(kName, base::Value(name));
  }

  if (!client_id.empty()) {
    base::Value* clients =
        whitelist_dict->FindKeyOfType(kClients, base::Value::Type::LIST);
    if (!clients) {
      DCHECK(newly_added);
      clients = whitelist_dict->SetKey(kClients,
                                       base::Value(base::Value::Type::LIST));
    }

    base::Value client(client_id);
    DCHECK(!base::ContainsValue(clients->GetList(), client));
    clients->GetList().push_back(std::move(client));
  }

  if (!newly_added) {
    // Sanity-check that the stored name is equal to the name passed in.
    // In release builds this is a no-op.
    DCHECK_EQ(name, whitelist_dict->FindKey(kName)->GetString());
    return;
  }

  RegisterNewComponent(crx_id, name);
}

void SupervisedUserWhitelistInstallerImpl::UnregisterWhitelist(
    const std::string& client_id,
    const std::string& crx_id) {
  DictionaryPrefUpdate update(local_state_,
                              prefs::kRegisteredSupervisedUserWhitelists);
  bool removed = UnregisterWhitelistInternal(update.Get(), client_id, crx_id);
  DCHECK(removed);
}

void SupervisedUserWhitelistInstallerImpl::OnProfileWillBeRemoved(
    const base::FilePath& profile_path) {
  std::string client_id = ClientIdForProfilePath(profile_path);

  // Go through all registered whitelists and possibly unregister them for this
  // client. Because unregistering a whitelist might completely uninstall it, we
  // need to make a copy of all the IDs before iterating over them.
  DictionaryPrefUpdate update(local_state_,
                              prefs::kRegisteredSupervisedUserWhitelists);
  base::DictionaryValue* pref_dict = update.Get();

  std::vector<std::string> crx_ids;
  for (base::DictionaryValue::Iterator it(*pref_dict); !it.IsAtEnd();
       it.Advance()) {
    crx_ids.push_back(it.key());
  }

  for (const std::string& crx_id : crx_ids)
    UnregisterWhitelistInternal(pref_dict, client_id, crx_id);
}

}  // namespace

// static
std::unique_ptr<SupervisedUserWhitelistInstaller>
SupervisedUserWhitelistInstaller::Create(
    ComponentUpdateService* cus,
    ProfileAttributesStorage* profile_attributes_storage,
    PrefService* local_state) {
  return std::make_unique<SupervisedUserWhitelistInstallerImpl>(
      cus, profile_attributes_storage, local_state);
}

// static
void SupervisedUserWhitelistInstaller::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kRegisteredSupervisedUserWhitelists);
}

// static
std::string SupervisedUserWhitelistInstaller::ClientIdForProfilePath(
    const base::FilePath& profile_path) {
  // See ProfileInfoCache::CacheKeyFromProfilePath().
  // TODO(anthonyvd): update comment when the refactoring of ProfileInfoCache
  // is completed.
  return profile_path.BaseName().MaybeAsASCII();
}

// static
std::vector<uint8_t> SupervisedUserWhitelistInstaller::GetHashFromCrxId(
    const std::string& crx_id) {
  DCHECK(crx_file::id_util::IdIsValid(crx_id));

  std::vector<uint8_t> hash;
  uint8_t byte = 0;
  for (size_t i = 0; i < crx_id.size(); ++i) {
    // Uppercase characters in IDs are technically legal.
    int val = base::ToLowerASCII(crx_id[i]) - 'a';
    DCHECK_GE(val, 0);
    DCHECK_LT(val, 16);
    if (i % 2 == 0) {
      byte = val;
    } else {
      hash.push_back(16 * byte + val);
      byte = 0;
    }
  }
  return hash;
}

// static
void SupervisedUserWhitelistInstaller::TriggerComponentUpdate(
    OnDemandUpdater* updater,
    const std::string& crx_id) {
  // TODO(sorin): use a callback to check the result (crbug.com/639189).
  updater->OnDemandUpdate(crx_id, OnDemandUpdater::Priority::FOREGROUND,
                          component_updater::Callback());
}

}  // namespace component_updater
