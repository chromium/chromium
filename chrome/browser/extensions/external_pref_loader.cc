// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_pref_loader.h"

#include <set>
#include <utility>

#include "base/containers/contains.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/scoped_observation.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/value_iterators.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/user_type_filter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_file_task_runner.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_preferences/pref_service_syncable_observer.h"
#endif

using content::BrowserThread;

namespace {

constexpr base::FilePath::CharType kExternalExtensionJson[] =
    FILE_PATH_LITERAL("external_extensions.json");

// Extension installations are skipped here as excluding these in the overlay
// is a bit complicated.
// TODO(crbug.com/40658053) This is a temporary measure and should be replaced.
bool SkipInstallForChromeOSTablet(const base::FilePath& file_path) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!ash::switches::IsTabletFormFactor())
    return false;

  constexpr char const* kIdsNotToBeInstalledOnTabletFormFactor[] = {
      "blpcfgokakmgnkcojhhkbfbldkacnbeo.json",  // Youtube file name.
      "ejjicmeblgpmajnghnpcppodonldlgfn.json",  // Calendar file name.
      "hcglmfcclpfgljeaiahehebeoaiicbko.json",  // Google Photos file name.
      "lneaknkopdijkpnocmklfnjbeapigfbh.json",  // Google Maps file name.
      "pjkljhegncpnkpknbcohdijeoejaedia.json",  // Gmail file name.
  };

  return base::Contains(kIdsNotToBeInstalledOnTabletFormFactor,
                        file_path.BaseName().value());
#else
  return false;
#endif
}

std::set<base::FilePath> GetPrefsCandidateFilesFromFolder(
    const base::FilePath& external_extension_search_path) {
  std::set<base::FilePath> external_extension_paths;

  if (!base::PathExists(external_extension_search_path)) {
    // Does not have to exist.
    return external_extension_paths;
  }

  base::FileEnumerator json_files(
      external_extension_search_path,
      false,  // Recursive.
      base::FileEnumerator::FILES);
#if BUILDFLAG(IS_WIN)
  base::FilePath::StringType extension = base::UTF8ToWide(".json");
#elif BUILDFLAG(IS_POSIX)
  base::FilePath::StringType extension(".json");
#endif
  do {
    base::FilePath file = json_files.Next();
    if (file.BaseName().value() == kExternalExtensionJson)
      continue;  // Already taken care of elsewhere.
    if (file.empty())
      break;
    if (file.MatchesExtension(extension)) {
      if (SkipInstallForChromeOSTablet(file))
        continue;
      external_extension_paths.insert(file.BaseName());
    } else {
      DVLOG(1) << "Not considering: " << file.LossyDisplayName()
               << " (does not have a .json extension)";
    }
  } while (true);

  return external_extension_paths;
}

}  // namespace

namespace extensions {

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Helper class to wait for priority pref sync to be ready.
class ExternalPrefLoader::PrioritySyncReadyWaiter
    : public sync_preferences::PrefServiceSyncableObserver,
      public syncer::SyncServiceObserver {
 public:
  explicit PrioritySyncReadyWaiter(Profile* profile) : profile_(profile) {
    DCHECK(profile_);
  }

  PrioritySyncReadyWaiter(const PrioritySyncReadyWaiter&) = delete;
  PrioritySyncReadyWaiter& operator=(const PrioritySyncReadyWaiter&) = delete;

  ~PrioritySyncReadyWaiter() override = default;

  void Start(base::OnceClosure done_closure) {
    if (IsPrioritySyncing()) {
      std::move(done_closure).Run();
      // Note: |this| is deleted here.
      return;
    }
    DCHECK(!done_closure_);
    done_closure_ = std::move(done_closure);
    MaybeObserveSyncStart();
  }

 private:
  void MaybeObserveSyncStart() {
    syncer::SyncService* service = SyncServiceFactory::GetForProfile(profile_);
    if (!service || !service->IsSyncFeatureEnabled()) {
      Finish();
      // Note: |this| is deleted.
      return;
    }
    AddObservers();
  }

  // sync_preferences::PrefServiceSyncableObserver:
  void OnIsSyncingChanged() override {
    DCHECK(profile_);
    if (!IsPrioritySyncing())
      return;

    Finish();
    // Note: |this| is deleted here.
  }

  // syncer::SyncServiceObserver
  void OnStateChanged(syncer::SyncService* sync) override {
    if (!sync->IsSyncFeatureEnabled()) {
      Finish();
    }
  }

  void OnSyncShutdown(syncer::SyncService* sync) override {
    DCHECK(sync_service_observation_.IsObservingSource(sync));
    sync_service_observation_.Reset();
  }

  bool IsPrioritySyncing() {
    sync_preferences::PrefServiceSyncable* prefs =
        PrefServiceSyncableFromProfile(profile_);
    return prefs->AreOsPriorityPrefsSyncing();
  }

  void AddObservers() {
    sync_preferences::PrefServiceSyncable* prefs =
        PrefServiceSyncableFromProfile(profile_);
    DCHECK(prefs);
    syncable_pref_observation_.Observe(prefs);

    syncer::SyncService* service = SyncServiceFactory::GetForProfile(profile_);
    sync_service_observation_.Observe(service);
  }

  void Finish() { std::move(done_closure_).Run(); }

  raw_ptr<Profile, LeakedDanglingUntriaged> profile_;

  base::OnceClosure done_closure_;

  // Used for registering observer for sync_preferences::PrefServiceSyncable.
  base::ScopedObservation<sync_preferences::PrefServiceSyncable,
                          sync_preferences::PrefServiceSyncableObserver>
      syncable_pref_observation_{this};
  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observation_{this};
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

ExternalPrefLoader::ExternalPrefLoader(int base_path_id,
                                       int options,
                                       Profile* profile)
    : base_path_id_(base_path_id),
      options_(options),
      profile_(profile),
      user_type_(profile ? apps::DetermineUserType(profile) : std::string()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

ExternalPrefLoader::~ExternalPrefLoader() {
}

const base::FilePath ExternalPrefLoader::GetBaseCrxFilePath() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // |base_path_| was set in LoadOnFileThread().
  return base_path_;
}

void ExternalPrefLoader::StartLoading() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if ((options_ & DELAY_LOAD_UNTIL_PRIORITY_SYNC) &&
      (profile_ && SyncServiceFactory::IsSyncAllowed(profile_))) {
    pending_waiter_list_.push_back(
        std::make_unique<PrioritySyncReadyWaiter>(profile_));
    PrioritySyncReadyWaiter* waiter_ptr = pending_waiter_list_.back().get();
    waiter_ptr->Start(base::BindOnce(&ExternalPrefLoader::OnPrioritySyncReady,
                                     this, waiter_ptr));
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  GetExtensionFileTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ExternalPrefLoader::LoadOnFileThread, this));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void ExternalPrefLoader::OnPrioritySyncReady(
    ExternalPrefLoader::PrioritySyncReadyWaiter* waiter) {
  // Delete |waiter| from |pending_waiter_list_|.
  pending_waiter_list_.erase(
      base::ranges::find(pending_waiter_list_, waiter,
                         &std::unique_ptr<PrioritySyncReadyWaiter>::get));
  // Continue loading.
  GetExtensionFileTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ExternalPrefLoader::LoadOnFileThread, this));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// static.
base::Value::Dict ExternalPrefLoader::ExtractExtensionPrefs(
    base::ValueDeserializer* deserializer,
    const base::FilePath& path) {
  std::string error_msg;
  std::unique_ptr<base::Value> extensions =
      deserializer->Deserialize(nullptr, &error_msg);
  if (!extensions) {
    LOG(WARNING) << "Unable to deserialize json data: " << error_msg
                 << " in file " << path.value() << ".";
    return base::Value::Dict();
  }

  if (extensions->is_dict())
    return std::move(*extensions).TakeDict();

  LOG(WARNING) << "Expected a JSON dictionary in file " << path.value() << ".";
  return base::Value::Dict();
}

void ExternalPrefLoader::LoadOnFileThread() {
  base::Value::Dict prefs;

  // TODO(skerner): Some values of base_path_id_ will cause
  // base::PathService::Get() to return false, because the path does
  // not exist.  Find and fix the build/install scripts so that
  // this can become a CHECK().  Known examples include chrome
  // OS developer builds and linux install packages.
  // Tracked as crbug.com/70402 .
  if (base::PathService::Get(base_path_id_, &base_path_)) {
    ReadExternalExtensionPrefFile(prefs);

    if (!prefs.empty())
      LOG(WARNING) << "You are using an old-style extension deployment method "
                      "(external_extensions.json), which will soon be "
                      "deprecated. (see http://developer.chrome.com/"
                      "docs/extensions/how-to/distribute/install-extensions)";

    ReadStandaloneExtensionPrefFiles(prefs);
  }

  // If we have any records to process, then we must have
  // read at least one .json file.  If so, then we should have
  // set |base_path_|.
  if (!prefs.empty())
    CHECK(!base_path_.empty());

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&ExternalPrefLoader::LoadFinished, this,
                                std::move(prefs)));
}

void ExternalPrefLoader::ReadExternalExtensionPrefFile(
    base::Value::Dict& prefs) {
  base::FilePath json_file = base_path_.Append(kExternalExtensionJson);

  if (!base::PathExists(json_file)) {
    // This is not an error.  The file does not exist by default.
    return;
  }

  if (IsOptionSet(ENSURE_PATH_CONTROLLED_BY_ADMIN)) {
#if BUILDFLAG(IS_MAC)
    if (!base::VerifyPathControlledByAdmin(json_file)) {
      LOG(ERROR) << "Can not read external extensions source.  The file "
                 << json_file.value() << " and every directory in its path, "
                 << "must be owned by root, have group \"admin\", and not be "
                 << "writable by all users. These restrictions prevent "
                 << "unprivleged users from making chrome install extensions "
                 << "on other users' accounts.";
      return;
    }
#else
    // The only platform that uses this check is Mac OS.  If you add one,
    // you need to implement base::VerifyPathControlledByAdmin() for
    // that platform.
    NOTREACHED_IN_MIGRATION();
#endif  // BUILDFLAG(IS_MAC)
  }

  JSONFileValueDeserializer deserializer(json_file);
  auto ext_prefs = ExtractExtensionPrefs(&deserializer, json_file);
  prefs.Merge(std::move(ext_prefs));
}

void ExternalPrefLoader::ReadStandaloneExtensionPrefFiles(
    base::Value::Dict& prefs) {
  // First list the potential .json candidates.
  std::set<base::FilePath> candidates =
      GetPrefsCandidateFilesFromFolder(base_path_);
  if (candidates.empty()) {
    DVLOG(1) << "Extension candidates list empty";
    return;
  }

  // TODO(crbug.com/40887866): Remove this once migration is completed.
  std::unique_ptr<base::Value::List> default_user_types;
  if (options_ & USE_USER_TYPE_PROFILE_FILTER) {
    default_user_types = std::make_unique<base::Value::List>();
    default_user_types->Append(base::Value(apps::kUserTypeUnmanaged));
  }

  // For each file read the json description & build the proper
  // associated prefs.
  for (auto it = candidates.begin(); it != candidates.end(); ++it) {
    base::FilePath extension_candidate_path = base_path_.Append(*it);

    const std::string id =
#if BUILDFLAG(IS_WIN)
        base::WideToASCII(
            extension_candidate_path.RemoveExtension().BaseName().value());
#elif BUILDFLAG(IS_POSIX)
        extension_candidate_path.RemoveExtension().BaseName().value();
#endif

    DVLOG(1) << "Reading json file: "
             << extension_candidate_path.LossyDisplayName();

    JSONFileValueDeserializer deserializer(extension_candidate_path);
    auto ext_prefs =
        ExtractExtensionPrefs(&deserializer, extension_candidate_path);

    if (options_ & USE_USER_TYPE_PROFILE_FILTER &&
        !apps::UserTypeMatchesJsonUserType(
            user_type_, id /* app_id */, ext_prefs, default_user_types.get())) {
      // Already logged.
      continue;
    }

    DVLOG(1) << "Adding extension with id: " << id;
    prefs.Set(id, std::move(ext_prefs));
  }
}

}  // namespace extensions
