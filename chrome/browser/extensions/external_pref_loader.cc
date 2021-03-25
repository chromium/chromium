// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_pref_loader.h"

#include <utility>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/scoped_observation.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
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
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_observer.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_preferences/pref_service_syncable_observer.h"
#endif

using content::BrowserThread;

namespace {

constexpr base::FilePath::CharType kExternalExtensionJson[] =
    FILE_PATH_LITERAL("external_extensions.json");

// Extension installations are skipped here as excluding these in the overlay
// is a bit complicated.
// TODO(crbug.com/1023268) This is a temporary measure and should be replaced.
bool SkipInstallForChromeOSTablet(const base::FilePath& file_path) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!chromeos::switches::IsTabletFormFactor())
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
#if defined(OS_WIN)
  base::FilePath::StringType extension = base::UTF8ToWide(".json");
#elif defined(OS_POSIX)
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

  ~PrioritySyncReadyWaiter() override = default;

  void Start(base::OnceClosure done_closure) {
    if (IsPrioritySyncing()) {
      std::move(done_closure).Run();
      // Note: |this| is deleted here.
      return;
    }
    DCHECK(!done_closure_);
    done_closure_ = std::move(done_closure);
    if (chromeos::features::IsSplitSettingsSyncEnabled()) {
      // SplitSettingsSync lets users opt-out of sync during OOBE.
      PrefService* prefs = profile_->GetPrefs();
      if (!prefs->GetBoolean(chromeos::prefs::kSyncOobeCompleted)) {
        // Need to wait for OOBE completion before checking if sync is enabled.
        pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
        pref_change_registrar_->Init(prefs);
        // base::Unretained is safe because we own |pref_changed_registrar_|.
        pref_change_registrar_->Add(
            chromeos::prefs::kSyncOobeCompleted,
            base::BindRepeating(&PrioritySyncReadyWaiter::OnSyncOobeCompleted,
                                base::Unretained(this)));
        return;
      }
    }
    MaybeObserveSyncStart();
  }

 private:
  void OnSyncOobeCompleted() {
    DCHECK(chromeos::features::IsSplitSettingsSyncEnabled());
    DCHECK(
        profile_->GetPrefs()->GetBoolean(chromeos::prefs::kSyncOobeCompleted));
    pref_change_registrar_.reset();
    syncer::SyncService* service =
        ProfileSyncServiceFactory::GetForProfile(profile_);
    if (!service->GetUserSettings()->IsOsSyncFeatureEnabled()) {
      // User opted-out of OS sync, OS sync will never start, we're done here.
      Finish();
      // Note: |this| is deleted.
      return;
    }
    MaybeObserveSyncStart();
  }

  void MaybeObserveSyncStart() {
    syncer::SyncService* service =
        ProfileSyncServiceFactory::GetForProfile(profile_);
    DCHECK(service);
    if (!service->CanSyncFeatureStart()) {
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
    if (!sync->CanSyncFeatureStart())
      Finish();
  }

  void OnSyncShutdown(syncer::SyncService* sync) override {
    DCHECK(sync_service_observation_.IsObservingSource(sync));
    sync_service_observation_.Reset();
  }

  bool IsPrioritySyncing() {
    sync_preferences::PrefServiceSyncable* prefs =
        PrefServiceSyncableFromProfile(profile_);
    DCHECK(prefs);
    // SplitSettingsSync moves prefs like language and keyboard/mouse config to
    // OS priority prefs.
    return chromeos::features::IsSplitSettingsSyncEnabled()
               ? prefs->AreOsPriorityPrefsSyncing()
               : prefs->IsPrioritySyncing();
  }

  void AddObservers() {
    sync_preferences::PrefServiceSyncable* prefs =
        PrefServiceSyncableFromProfile(profile_);
    DCHECK(prefs);
    syncable_pref_observation_.Observe(prefs);

    syncer::SyncService* service =
        ProfileSyncServiceFactory::GetForProfile(profile_);
    sync_service_observation_.Observe(service);
  }

  void Finish() { std::move(done_closure_).Run(); }

  Profile* profile_;

  base::OnceClosure done_closure_;

  // Used with SplitSettingsSync to wait for OOBE sync dialog completion.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Used for registering observer for sync_preferences::PrefServiceSyncable.
  base::ScopedObservation<sync_preferences::PrefServiceSyncable,
                          sync_preferences::PrefServiceSyncableObserver>
      syncable_pref_observation_{this};
  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observation_{this};

  DISALLOW_COPY_AND_ASSIGN(PrioritySyncReadyWaiter);
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
      (profile_ && ProfileSyncServiceFactory::IsSyncAllowed(profile_))) {
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
      std::find_if(pending_waiter_list_.begin(), pending_waiter_list_.end(),
                   [waiter](const std::unique_ptr<PrioritySyncReadyWaiter>& w) {
                     return w.get() == waiter;
                   }));
  // Continue loading.
  GetExtensionFileTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ExternalPrefLoader::LoadOnFileThread, this));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// static.
std::unique_ptr<base::DictionaryValue>
ExternalPrefLoader::ExtractExtensionPrefs(base::ValueDeserializer* deserializer,
                                          const base::FilePath& path) {
  std::string error_msg;
  std::unique_ptr<base::Value> extensions =
      deserializer->Deserialize(NULL, &error_msg);
  if (!extensions) {
    LOG(WARNING) << "Unable to deserialize json data: " << error_msg
                 << " in file " << path.value() << ".";
    return std::make_unique<base::DictionaryValue>();
  }

  std::unique_ptr<base::DictionaryValue> ext_dictionary =
      base::DictionaryValue::From(std::move(extensions));
  if (ext_dictionary)
    return ext_dictionary;

  LOG(WARNING) << "Expected a JSON dictionary in file " << path.value() << ".";
  return std::make_unique<base::DictionaryValue>();
}

void ExternalPrefLoader::LoadOnFileThread() {
  auto prefs = std::make_unique<base::DictionaryValue>();

  // TODO(skerner): Some values of base_path_id_ will cause
  // base::PathService::Get() to return false, because the path does
  // not exist.  Find and fix the build/install scripts so that
  // this can become a CHECK().  Known examples include chrome
  // OS developer builds and linux install packages.
  // Tracked as crbug.com/70402 .
  if (base::PathService::Get(base_path_id_, &base_path_)) {
    ReadExternalExtensionPrefFile(prefs.get());

    if (!prefs->empty())
      LOG(WARNING) << "You are using an old-style extension deployment method "
                      "(external_extensions.json), which will soon be "
                      "deprecated. (see http://developer.chrome.com/"
                      "extensions/external_extensions.html)";

    ReadStandaloneExtensionPrefFiles(prefs.get());
  }

  if (base_path_id_ == chrome::DIR_EXTERNAL_EXTENSIONS)
    UMA_HISTOGRAM_COUNTS_100("Extensions.ExternalJsonCount", prefs->size());

  // If we have any records to process, then we must have
  // read at least one .json file.  If so, then we should have
  // set |base_path_|.
  if (!prefs->empty())
    CHECK(!base_path_.empty());

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&ExternalPrefLoader::LoadFinished, this,
                                std::move(prefs)));
}

void ExternalPrefLoader::ReadExternalExtensionPrefFile(
    base::DictionaryValue* prefs) {
  CHECK(NULL != prefs);

  base::FilePath json_file = base_path_.Append(kExternalExtensionJson);

  if (!base::PathExists(json_file)) {
    // This is not an error.  The file does not exist by default.
    return;
  }

  if (IsOptionSet(ENSURE_PATH_CONTROLLED_BY_ADMIN)) {
#if defined(OS_MAC)
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
    NOTREACHED();
#endif  // defined(OS_MAC)
  }

  JSONFileValueDeserializer deserializer(json_file);
  std::unique_ptr<base::DictionaryValue> ext_prefs =
      ExtractExtensionPrefs(&deserializer, json_file);
  if (ext_prefs)
    prefs->MergeDictionary(ext_prefs.get());
}

void ExternalPrefLoader::ReadStandaloneExtensionPrefFiles(
    base::DictionaryValue* prefs) {
  CHECK(NULL != prefs);

  // First list the potential .json candidates.
  std::set<base::FilePath> candidates =
      GetPrefsCandidateFilesFromFolder(base_path_);
  if (candidates.empty()) {
    DVLOG(1) << "Extension candidates list empty";
    return;
  }

  // TODO(crbug.com/1407498): Remove this once migration is completed.
  std::unique_ptr<base::ListValue> default_user_types;
  if (options_ & USE_USER_TYPE_PROFILE_FILTER) {
    default_user_types = std::make_unique<base::ListValue>();
    default_user_types->Append(base::Value(apps::kUserTypeUnmanaged));
  }

  // For each file read the json description & build the proper
  // associated prefs.
  for (auto it = candidates.begin(); it != candidates.end(); ++it) {
    base::FilePath extension_candidate_path = base_path_.Append(*it);

    const std::string id =
#if defined(OS_WIN)
        base::WideToASCII(
            extension_candidate_path.RemoveExtension().BaseName().value());
#elif defined(OS_POSIX)
        extension_candidate_path.RemoveExtension().BaseName().value();
#endif

    DVLOG(1) << "Reading json file: "
             << extension_candidate_path.LossyDisplayName();

    JSONFileValueDeserializer deserializer(extension_candidate_path);
    std::unique_ptr<base::DictionaryValue> ext_prefs =
        ExtractExtensionPrefs(&deserializer, extension_candidate_path);
    if (!ext_prefs)
      continue;

    if (options_ & USE_USER_TYPE_PROFILE_FILTER &&
        !apps::UserTypeMatchesJsonUserType(user_type_, id /* app_id */,
                                           ext_prefs.get(),
                                           default_user_types.get())) {
      // Already logged.
      continue;
    }

    DVLOG(1) << "Adding extension with id: " << id;
    prefs->Set(id, std::move(ext_prefs));
  }
}

}  // namespace extensions
