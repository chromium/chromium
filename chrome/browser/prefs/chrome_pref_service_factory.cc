// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/chrome_pref_service_factory.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/prefs/chrome_command_line_pref_store.h"
#include "chrome/browser/prefs/chrome_pref_model_associator_client.h"
#include "chrome/browser/prefs/profile_pref_store_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/sync_start_util.h"
#include "chrome/browser/ui/profile_error_dialog.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/component_updater/pref_names.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/default_pref_store.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_filter.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_store.h"
#include "components/prefs/pref_value_store.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/pref_names.h"
#include "components/sync_preferences/pref_model_associator.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_preferences/pref_service_syncable_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "rlz/buildflags/buildflags.h"
#include "services/preferences/public/cpp/tracked/configuration.h"
#include "services/preferences/public/cpp/tracked/pref_names.h"
#include "sql/error_delegate_util.h"
#include "ui/base/resource/resource_bundle.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/pref_names.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_pref_store.h"
#endif

#if defined(OS_WIN)
#include "base/win/win_util.h"
#if BUILDFLAG(ENABLE_RLZ)
#include "rlz/lib/machine_id.h"
#endif  // BUILDFLAG(ENABLE_RLZ)
#endif  // defined(OS_WIN)

using content::BrowserContext;
using content::BrowserThread;

using EnforcementLevel =
    prefs::mojom::TrackedPreferenceMetadata::EnforcementLevel;
using PrefTrackingStrategy =
    prefs::mojom::TrackedPreferenceMetadata::PrefTrackingStrategy;
using ValueType = prefs::mojom::TrackedPreferenceMetadata::ValueType;

namespace {

#if defined(OS_WIN)
// Whether we are in testing mode; can be enabled via
// DisableDomainCheckForTesting(). Forces startup checks to ignore the presence
// of a domain when determining the active SettingsEnforcement group.
bool g_disable_domain_check_for_testing = false;
#endif  // OS_WIN

// These preferences must be kept in sync with the TrackedPreference enum in
// tools/metrics/histograms/enums.xml. To add a new preference, append it to the
// array and add a corresponding value to the histogram enum. Each tracked
// preference must be given a unique reporting ID.
// See CleanupDeprecatedTrackedPreferences() in pref_hash_filter.cc to remove a
// deprecated tracked preference.
const prefs::TrackedPreferenceMetadata kTrackedPrefs[] = {
    {0, prefs::kShowHomeButton, EnforcementLevel::ENFORCE_ON_LOAD,
     PrefTrackingStrategy::ATOMIC, ValueType::IMPERSONAL},
    {1, prefs::kHomePageIsNewTabPage, EnforcementLevel::ENFORCE_ON_LOAD,
     PrefTrackingStrategy::ATOMIC, ValueType::IMPERSONAL},
    {2, prefs::kHomePage, EnforcementLevel::ENFORCE_ON_LOAD,
     PrefTrackingStrategy::ATOMIC, ValueType::IMPERSONAL},
    {3, prefs::kRestoreOnStartup, EnforcementLevel::ENFORCE_ON_LOAD,
     PrefTrackingStrategy::ATOMIC, ValueType::IMPERSONAL},
    {4, prefs::kURLsToRestoreOnStartup, EnforcementLevel::ENFORCE_ON_LOAD,
     PrefTrackingStrategy::ATOMIC, ValueType::IMPERSONAL},
#if BUILDFLAG(ENABLE_EXTENSIONS)
    {5, extensions::pref_names::kExtensions, EnforcementLevel::NO_ENFORCEMENT,
     PrefTrackingStrategy::SPLIT, ValueType::IMPERSONAL},
#endif
    {6, prefs::kGoogleServicesLastUsername, EnforcementLevel::ENFORCE_ON_LOAD,
     PrefTrackingStrategy::ATOMIC, ValueType::PERSONAL},
    {7, prefs::kSearchProviderOverrides, EnforcementLevel::ENFORCE_ON_LOAD,
     PrefTrackingStrategy::ATOMIC, ValueType::IMPERSONAL},
#if !defined(OS_ANDROID)
    {11, prefs::kPinnedTabs, EnforcementLevel::ENFORCE_ON_LOAD,
     PrefTrackingStrategy::ATOMIC, ValueType::IMPERSONAL},
#endif
    {14, DefaultSearchManager::kDefaultSearchProviderDataPrefName,
     EnforcementLevel::NO_ENFORCEMENT, PrefTrackingStrategy::ATOMIC,
     ValueType::IMPERSONAL},
    {// Protecting kPreferenceResetTime does two things:
     //  1) It ensures this isn't accidently set by someone stomping the pref
     //     file.
     //  2) More importantly, it declares kPreferenceResetTime as a protected
     //     pref which is required for it to be visible when queried via the
     //     SegregatedPrefStore. This is because it's written directly in the
     //     protected JsonPrefStore by that store's PrefHashFilter if there was
     //     a reset in FilterOnLoad and SegregatedPrefStore will not look for it
     //     in the protected JsonPrefStore unless it's declared as a protected
     //     preference here.
     15, user_prefs::kPreferenceResetTime, EnforcementLevel::ENFORCE_ON_LOAD,
     PrefTrackingStrategy::ATOMIC, ValueType::IMPERSONAL},
    // kSyncRemainingRollbackTries is deprecated and will be removed a few
    // releases after M50.
    {18, prefs::kSafeBrowsingIncidentsSent, EnforcementLevel::ENFORCE_ON_LOAD,
     PrefTrackingStrategy::ATOMIC, ValueType::IMPERSONAL},
#if defined(OS_WIN)
    {19, prefs::kSwReporterPromptVersion, EnforcementLevel::ENFORCE_ON_LOAD,
     PrefTrackingStrategy::ATOMIC, ValueType::IMPERSONAL},
#endif
    // This pref is deprecated and will be removed a few releases after M43.
    // kGoogleServicesAccountId replaces it.
    {21, prefs::kGoogleServicesUsername, EnforcementLevel::ENFORCE_ON_LOAD,
     PrefTrackingStrategy::ATOMIC, ValueType::PERSONAL},
#if defined(OS_WIN)
    {22, prefs::kSwReporterPromptSeed, EnforcementLevel::ENFORCE_ON_LOAD,
     PrefTrackingStrategy::ATOMIC, ValueType::IMPERSONAL},
#endif
    {23, prefs::kGoogleServicesAccountId, EnforcementLevel::ENFORCE_ON_LOAD,
     PrefTrackingStrategy::ATOMIC, ValueType::PERSONAL},
    {24, prefs::kGoogleServicesLastAccountId, EnforcementLevel::ENFORCE_ON_LOAD,
     PrefTrackingStrategy::ATOMIC, ValueType::PERSONAL},
#if defined(OS_WIN)
    {25, prefs::kSettingsResetPromptPromptWave,
     EnforcementLevel::ENFORCE_ON_LOAD, PrefTrackingStrategy::ATOMIC,
     ValueType::IMPERSONAL},
    {26, prefs::kSettingsResetPromptLastTriggeredForDefaultSearch,
     EnforcementLevel::ENFORCE_ON_LOAD, PrefTrackingStrategy::ATOMIC,
     ValueType::IMPERSONAL},
    {27, prefs::kSettingsResetPromptLastTriggeredForStartupUrls,
     EnforcementLevel::ENFORCE_ON_LOAD, PrefTrackingStrategy::ATOMIC,
     ValueType::IMPERSONAL},
    {28, prefs::kSettingsResetPromptLastTriggeredForHomepage,
     EnforcementLevel::ENFORCE_ON_LOAD, PrefTrackingStrategy::ATOMIC,
     ValueType::IMPERSONAL},
#endif  // defined(OS_WIN)
    {29, prefs::kMediaStorageIdSalt, EnforcementLevel::ENFORCE_ON_LOAD,
     PrefTrackingStrategy::ATOMIC, ValueType::IMPERSONAL},
#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
    {30, prefs::kModuleBlacklistCacheMD5Digest,
     EnforcementLevel::ENFORCE_ON_LOAD, PrefTrackingStrategy::ATOMIC,
     ValueType::IMPERSONAL},
#endif

    // See note at top, new items added here also need to be added to
    // histograms.xml's TrackedPreference enum.
};

// One more than the last tracked preferences ID above.
const size_t kTrackedPrefsReportingIDsCount =
    kTrackedPrefs[arraysize(kTrackedPrefs) - 1].reporting_id + 1;

// Each group enforces a superset of the protection provided by the previous
// one.
enum SettingsEnforcementGroup {
  GROUP_NO_ENFORCEMENT,
  // Enforce protected settings on profile loads.
  GROUP_ENFORCE_ALWAYS,
  // Also enforce extension default search.
  GROUP_ENFORCE_ALWAYS_WITH_DSE,
  // Also enforce extension settings and default search.
  GROUP_ENFORCE_ALWAYS_WITH_EXTENSIONS_AND_DSE,
  // The default enforcement group contains all protection features.
  GROUP_ENFORCE_DEFAULT
};

SettingsEnforcementGroup GetSettingsEnforcementGroup() {
# if defined(OS_WIN)
  if (!g_disable_domain_check_for_testing) {
    static bool first_call = true;
    static const bool is_managed = base::win::IsEnterpriseManaged();
    if (first_call) {
      UMA_HISTOGRAM_BOOLEAN("Settings.TrackedPreferencesNoEnforcementOnDomain",
                            is_managed);
      first_call = false;
    }
    if (is_managed)
      return GROUP_NO_ENFORCEMENT;
  }
#endif

  struct {
    const char* group_name;
    SettingsEnforcementGroup group;
  } static const kEnforcementLevelMap[] = {
    { chrome_prefs::internals::kSettingsEnforcementGroupNoEnforcement,
      GROUP_NO_ENFORCEMENT },
    { chrome_prefs::internals::kSettingsEnforcementGroupEnforceAlways,
      GROUP_ENFORCE_ALWAYS },
    { chrome_prefs::internals::
          kSettingsEnforcementGroupEnforceAlwaysWithDSE,
      GROUP_ENFORCE_ALWAYS_WITH_DSE },
    { chrome_prefs::internals::
          kSettingsEnforcementGroupEnforceAlwaysWithExtensionsAndDSE,
      GROUP_ENFORCE_ALWAYS_WITH_EXTENSIONS_AND_DSE },
  };

  // Use the strongest enforcement setting in the absence of a field trial
  // config on Windows and MacOS. Remember to update the OFFICIAL_BUILD section
  // of extension_startup_browsertest.cc and pref_hash_browsertest.cc when
  // updating the default value below.
  // TODO(gab): Enforce this on all platforms.
  SettingsEnforcementGroup enforcement_group =
#if defined(OS_WIN) || defined(OS_MACOSX)
      GROUP_ENFORCE_DEFAULT;
#else
      GROUP_NO_ENFORCEMENT;
#endif
  bool group_determined_from_trial = false;
  base::FieldTrial* trial =
      base::FieldTrialList::Find(
          chrome_prefs::internals::kSettingsEnforcementTrialName);
  if (trial) {
    const std::string& group_name = trial->group_name();
    for (size_t i = 0; i < arraysize(kEnforcementLevelMap); ++i) {
      if (kEnforcementLevelMap[i].group_name == group_name) {
        enforcement_group = kEnforcementLevelMap[i].group;
        group_determined_from_trial = true;
        break;
      }
    }
  }
  UMA_HISTOGRAM_BOOLEAN("Settings.EnforcementGroupDeterminedFromTrial",
                        group_determined_from_trial);
  return enforcement_group;
}

// Returns the effective preference tracking configuration.
std::vector<prefs::mojom::TrackedPreferenceMetadataPtr>
GetTrackingConfiguration() {
  const SettingsEnforcementGroup enforcement_group =
      GetSettingsEnforcementGroup();

  std::vector<prefs::mojom::TrackedPreferenceMetadataPtr> result;
  for (size_t i = 0; i < arraysize(kTrackedPrefs); ++i) {
    prefs::mojom::TrackedPreferenceMetadataPtr data =
        prefs::ConstructTrackedMetadata(kTrackedPrefs[i]);

    if (GROUP_NO_ENFORCEMENT == enforcement_group) {
      // Remove enforcement for all tracked preferences.
      data->enforcement_level = EnforcementLevel::NO_ENFORCEMENT;
    }

    if (enforcement_group >= GROUP_ENFORCE_ALWAYS_WITH_DSE &&
        data->name ==
            DefaultSearchManager::kDefaultSearchProviderDataPrefName) {
      // Specifically enable default search settings enforcement.
      data->enforcement_level = EnforcementLevel::ENFORCE_ON_LOAD;
    }

#if BUILDFLAG(ENABLE_EXTENSIONS)
    if (enforcement_group >= GROUP_ENFORCE_ALWAYS_WITH_EXTENSIONS_AND_DSE &&
        data->name == extensions::pref_names::kExtensions) {
      // Specifically enable extension settings enforcement.
      data->enforcement_level = EnforcementLevel::ENFORCE_ON_LOAD;
    }
#endif

    result.push_back(std::move(data));
  }
  return result;
}

std::unique_ptr<ProfilePrefStoreManager> CreateProfilePrefStoreManager(
    const base::FilePath& profile_path) {
  std::string legacy_device_id;
#if defined(OS_WIN) && BUILDFLAG(ENABLE_RLZ)
  // This is used by
  // chrome/browser/apps/platform_apps/api/music_manager_private/device_id_win.cc
  // but that API is private (http://crbug.com/276485) and other platforms are
  // not available synchronously.
  // As part of improving pref metrics on other platforms we may want to find
  // ways to defer preference loading until the device ID can be used.
  rlz_lib::GetMachineId(&legacy_device_id);

  UMA_HISTOGRAM_BOOLEAN("Settings.LegacyMachineIdGenerationSuccess",
                        !legacy_device_id.empty());
#endif
  std::string seed;
#if defined(GOOGLE_CHROME_BUILD)
  seed = ui::ResourceBundle::GetSharedInstance()
             .GetRawDataResource(IDR_PREF_HASH_SEED_BIN)
             .as_string();
#endif
  return std::make_unique<ProfilePrefStoreManager>(profile_path, seed,
                                                   legacy_device_id);
}

void PrepareFactory(sync_preferences::PrefServiceSyncableFactory* factory,
                    const base::FilePath& pref_filename,
                    policy::PolicyService* policy_service,
                    SupervisedUserSettingsService* supervised_user_settings,
                    scoped_refptr<PersistentPrefStore> user_pref_store,
                    scoped_refptr<PrefStore> extension_prefs,
                    bool async,
                    policy::BrowserPolicyConnector* policy_connector) {
  factory->SetManagedPolicies(policy_service, policy_connector);
  factory->SetRecommendedPolicies(policy_service, policy_connector);

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  if (supervised_user_settings) {
    scoped_refptr<PrefStore> supervised_user_prefs =
        base::MakeRefCounted<SupervisedUserPrefStore>(supervised_user_settings);
    DCHECK(async || supervised_user_prefs->IsInitializationComplete());
    factory->set_supervised_user_prefs(supervised_user_prefs);
  }
#endif

  factory->set_async(async);
  factory->set_extension_prefs(std::move(extension_prefs));
  factory->set_command_line_prefs(
      base::MakeRefCounted<ChromeCommandLinePrefStore>(
          base::CommandLine::ForCurrentProcess()));
  factory->set_read_error_callback(base::BindRepeating(
      &chrome_prefs::HandlePersistentPrefStoreReadError, pref_filename));
  factory->set_user_prefs(std::move(user_pref_store));
  factory->SetPrefModelAssociatorClient(
      ChromePrefModelAssociatorClient::GetInstance());
}

class ResetOnLoadObserverImpl : public prefs::mojom::ResetOnLoadObserver {
 public:
  explicit ResetOnLoadObserverImpl(const base::FilePath& profile_path)
      : profile_path_(profile_path) {}

  void OnResetOnLoad() override {
    // A StartSyncFlare used to kick sync early in case of a reset event. This
    // is done since sync may bring back the user's server value post-reset
    // which could potentially cause a "settings flash" between the factory
    // default and the re-instantiated server value. Starting sync ASAP
    // minimizes the window before the server value is re-instantiated (this
    // window can otherwise be as long as 10 seconds by default).
    sync_start_util::GetFlareForSyncableService(profile_path_)
        .Run(syncer::PREFERENCES);
  }

 private:
  const base::FilePath profile_path_;

  DISALLOW_COPY_AND_ASSIGN(ResetOnLoadObserverImpl);
};

}  // namespace

namespace chrome_prefs {

namespace internals {

// Group modifications should be reflected in first_run_browsertest.cc and
// pref_hash_browsertest.cc.
const char kSettingsEnforcementTrialName[] = "SettingsEnforcement";
const char kSettingsEnforcementGroupNoEnforcement[] = "no_enforcement";
const char kSettingsEnforcementGroupEnforceAlways[] = "enforce_always";
const char kSettingsEnforcementGroupEnforceAlwaysWithDSE[] =
    "enforce_always_with_dse";
const char kSettingsEnforcementGroupEnforceAlwaysWithExtensionsAndDSE[] =
    "enforce_always_with_extensions_and_dse";

}  // namespace internals

std::unique_ptr<PrefService> CreateLocalState(
    const base::FilePath& pref_filename,
    policy::PolicyService* policy_service,
    scoped_refptr<PrefRegistry> pref_registry,
    bool async,
    std::unique_ptr<PrefValueStore::Delegate> delegate,
    policy::BrowserPolicyConnector* policy_connector) {
  sync_preferences::PrefServiceSyncableFactory factory;
  PrepareFactory(&factory, pref_filename, policy_service,
                 nullptr,  // supervised_user_settings
                 base::MakeRefCounted<JsonPrefStore>(
                     pref_filename, std::unique_ptr<PrefFilter>()),
                 nullptr,  // extension_prefs
                 async, policy_connector);

  return factory.Create(std::move(pref_registry), std::move(delegate));
}

std::unique_ptr<sync_preferences::PrefServiceSyncable> CreateProfilePrefs(
    const base::FilePath& profile_path,
    prefs::mojom::TrackedPreferenceValidationDelegatePtr validation_delegate,
    policy::PolicyService* policy_service,
    SupervisedUserSettingsService* supervised_user_settings,
    scoped_refptr<PrefStore> extension_prefs,
    scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry,
    bool async,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    std::unique_ptr<PrefValueStore::Delegate> delegate) {
  TRACE_EVENT0("browser", "chrome_prefs::CreateProfilePrefs");
  SCOPED_UMA_HISTOGRAM_TIMER("PrefService.CreateProfilePrefsTime");

  prefs::mojom::ResetOnLoadObserverPtr reset_on_load_observer;
  mojo::MakeStrongBinding(
      std::make_unique<ResetOnLoadObserverImpl>(profile_path),
      mojo::MakeRequest(&reset_on_load_observer));
  sync_preferences::PrefServiceSyncableFactory factory;
  scoped_refptr<PersistentPrefStore> user_pref_store =
      CreateProfilePrefStoreManager(profile_path)
          ->CreateProfilePrefStore(
              GetTrackingConfiguration(), kTrackedPrefsReportingIDsCount,
              std::move(io_task_runner), std::move(reset_on_load_observer),
              std::move(validation_delegate));
  PrepareFactory(&factory, profile_path, policy_service,
                 supervised_user_settings, std::move(user_pref_store),
                 std::move(extension_prefs), async,
                 g_browser_process->browser_policy_connector());
  return factory.CreateSyncable(std::move(pref_registry), std::move(delegate));
}

void InstallPoliciesOnLocalState(
    PrefService* preexisting_local_state,
    policy::PolicyService* policy_service,
    std::unique_ptr<PrefValueStore::Delegate> delegate) {
  sync_preferences::PrefServiceSyncableFactory factory;
  policy::BrowserPolicyConnector* policy_connector =
      g_browser_process->browser_policy_connector();
  factory.SetManagedPolicies(policy_service, policy_connector);
  factory.SetRecommendedPolicies(policy_service, policy_connector);
  factory.ChangePrefValueStore(preexisting_local_state, std::move(delegate));
}

void DisableDomainCheckForTesting() {
#if defined(OS_WIN)
  g_disable_domain_check_for_testing = true;
#endif  // OS_WIN
}

bool InitializePrefsFromMasterPrefs(
    const base::FilePath& profile_path,
    std::unique_ptr<base::DictionaryValue> master_prefs) {
  return CreateProfilePrefStoreManager(profile_path)
      ->InitializePrefsFromMasterPrefs(GetTrackingConfiguration(),
                                       kTrackedPrefsReportingIDsCount,
                                       std::move(master_prefs));
}

base::Time GetResetTime(Profile* profile) {
  return ProfilePrefStoreManager::GetResetTime(profile->GetPrefs());
}

void ClearResetTime(Profile* profile) {
  ProfilePrefStoreManager::ClearResetTime(profile->GetPrefs());
}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  ProfilePrefStoreManager::RegisterProfilePrefs(registry);
}

void HandlePersistentPrefStoreReadError(
    const base::FilePath& pref_filename,
    PersistentPrefStore::PrefReadError error) {
  // The error callback is always invoked back on the main thread (which is
  // BrowserThread::UI unless called during early initialization before the main
  // thread is promoted to BrowserThread::UI).
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         !BrowserThread::IsThreadInitialized(BrowserThread::UI));

  // Sample the histogram also for the successful case in order to get a
  // baseline on the success rate in addition to the error distribution.
  UMA_HISTOGRAM_ENUMERATION("PrefService.ReadError", error,
                            PersistentPrefStore::PREF_READ_ERROR_MAX_ENUM);

  if (error != PersistentPrefStore::PREF_READ_ERROR_NONE) {
#if !defined(OS_CHROMEOS)
    // Failing to load prefs on startup is a bad thing(TM). See bug 38352 for
    // an example problem that this can cause.
    // Do some diagnosis and try to avoid losing data.
    int message_id = 0;
    if (error <= PersistentPrefStore::PREF_READ_ERROR_JSON_TYPE) {
      message_id = IDS_PREFERENCES_CORRUPT_ERROR;
    } else if (error != PersistentPrefStore::PREF_READ_ERROR_NO_FILE) {
      message_id = IDS_PREFERENCES_UNREADABLE_ERROR;
    }

    if (message_id) {
      // Note: ThreadTaskRunnerHandle() is usually BrowserThread::UI but during
      // early startup it can be ChromeBrowserMainParts::DeferringTaskRunner
      // which will forward to BrowserThread::UI when it's initialized.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&ShowProfileErrorDialog, ProfileErrorType::PREFERENCES,
                         message_id,
                         sql::GetCorruptFileDiagnosticsInfo(pref_filename)));
    }
#else
    // On ChromeOS error screen with message about broken local state
    // will be displayed.

    // A supplementary error message about broken local state - is included
    // in logs and user feedbacks.
    if (error != PersistentPrefStore::PREF_READ_ERROR_NONE &&
        error != PersistentPrefStore::PREF_READ_ERROR_NO_FILE) {
      LOG(ERROR) << "An error happened during prefs loading: " << error;
    }
#endif
  }
}

}  // namespace chrome_prefs
