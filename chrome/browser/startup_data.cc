// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/startup_data.h"

#include <string_view>

#include "base/files/file_path.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/chrome_feature_list_creator.h"
#include "chrome/browser/prefs/profile_pref_store_manager.h"
#include "chrome/common/channel_info.h"
#include "components/metrics/delegating_provider.h"
#include "components/metrics/entropy_state_provider.h"
#include "components/metrics/field_trials_provider.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/persistent_system_profile.h"
#include "components/metrics/version_utils.h"
#include "third_party/metrics_proto/system_profile.pb.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "chrome/browser/android/profile_key_startup_accessor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_builder.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/policy/schema_registry_service_builder.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/browser/profiles/chrome_browser_main_extra_parts_profiles.h"
#include "chrome/browser/profiles/pref_service_builder_utils.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/keyed_service/core/simple_factory_key.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_store.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/network_service_instance.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/preferences/public/mojom/tracked_preference_validation_delegate.mojom.h"

namespace {

base::FilePath GetProfilePath() {
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  return user_data_dir.AppendASCII(chrome::kInitialProfile);
}

}  // namespace

#endif

StartupData::StartupData()
    : chrome_feature_list_creator_(
          std::make_unique<ChromeFeatureListCreator>()) {}

StartupData::~StartupData() = default;

void StartupData::RecordCoreSystemProfile() {
  metrics::SystemProfileProto system_profile;
  metrics::MetricsLog::RecordCoreSystemProfile(
      metrics::GetVersionString(),
      metrics::AsProtobufChannel(chrome::GetChannel()),
      chrome::IsExtendedStableChannel(),
      chrome_feature_list_creator_->actual_locale(),
      metrics::GetAppPackageName(), &system_profile);

  metrics::DelegatingProvider delegating_provider;

  // TODO(hanxi): Create SyntheticTrialRegistry and pass it to
  // |field_trial_provider|.
  delegating_provider.RegisterMetricsProvider(
      std::make_unique<variations::FieldTrialsProvider>(nullptr,
                                                        std::string_view()));

  // Persists low entropy source values.
  delegating_provider.RegisterMetricsProvider(
      std::make_unique<metrics::EntropyStateProvider>(
          chrome_feature_list_creator_->local_state()));

  delegating_provider.ProvideSystemProfileMetricsWithLogCreationTime(
      base::TimeTicks(), &system_profile);

  // TODO(crbug.com/40628379): Records information from other providers.
  metrics::GlobalPersistentSystemProfile::GetInstance()->SetSystemProfile(
      system_profile, /* complete */ false);
}

#if BUILDFLAG(IS_ANDROID)
void StartupData::InitProfileKey() {
  key_ = std::make_unique<ProfileKey>(GetProfilePath());
  PreProfilePrefServiceInit();

  ProfileKeyStartupAccessor::GetInstance()->SetProfileKey(key_.get());
}

void StartupData::CreateProfilePrefService() {
  CHECK(key_);
  CreateServicesInternal();
  key_->SetPrefs(prefs_.get());
}

bool StartupData::HasBuiltProfilePrefService() {
  return !!prefs_.get();
}

ProfileKey* StartupData::GetProfileKey() {
  return key_.get();
}

std::unique_ptr<ProfileKey> StartupData::TakeProfileKey() {
  return std::move(key_);
}

std::unique_ptr<policy::SchemaRegistryService>
StartupData::TakeSchemaRegistryService() {
  return std::move(schema_registry_service_);
}

std::unique_ptr<policy::UserCloudPolicyManager>
StartupData::TakeUserCloudPolicyManager() {
  return std::move(user_cloud_policy_manager_);
}

std::unique_ptr<policy::ProfilePolicyConnector>
StartupData::TakeProfilePolicyConnector() {
  return std::move(profile_policy_connector_);
}

scoped_refptr<user_prefs::PrefRegistrySyncable>
StartupData::TakePrefRegistrySyncable() {
  return std::move(pref_registry_);
}

std::unique_ptr<sync_preferences::PrefServiceSyncable>
StartupData::TakeProfilePrefService() {
  return std::move(prefs_);
}

std::unique_ptr<leveldb_proto::ProtoDatabaseProvider>
StartupData::TakeProtoDatabaseProvider() {
  return std::move(proto_db_provider_);
}

void StartupData::PreProfilePrefServiceInit() {
  pref_registry_ = base::MakeRefCounted<user_prefs::PrefRegistrySyncable>();
  ChromeBrowserMainExtraPartsProfiles::
      EnsureBrowserContextKeyedServiceFactoriesBuilt();

  const base::FilePath& path = key_->GetPath();
  if (!base::PathExists(path)) {
    // TODO(rogerta): http://crbug/160553 - Bad things happen if we can't
    // write to the profile directory.  We should eventually be able to run in
    // this situation.
    if (!base::CreateDirectory(path)) {
      return;
    }

    CreateProfileReadme(path);
  }

  // StoragePartitionImplMap uses profile directory as default storage
  // partition, see StoragePartitionImplMap::GetStoragePartitionPath().
  proto_db_provider_ = std::make_unique<leveldb_proto::ProtoDatabaseProvider>(
      path, /*is_in_memory=*/false);
  key_->SetProtoDatabaseProvider(proto_db_provider_.get());
}

void StartupData::CreateServicesInternal() {
  const base::FilePath& path = key_->GetPath();

  scoped_refptr<base::SequencedTaskRunner> io_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskShutdownBehavior::BLOCK_SHUTDOWN, base::MayBlock()});

  policy::ChromeBrowserPolicyConnector* browser_policy_connector =
      g_browser_process->browser_policy_connector();
  CHECK(browser_policy_connector);
  std::unique_ptr<policy::SchemaRegistry> schema_registry =
      std::make_unique<policy::SchemaRegistry>();
  schema_registry_service_ = BuildSchemaRegistryService(
      std::move(schema_registry), browser_policy_connector->GetChromeSchema(),
      browser_policy_connector->GetSchemaRegistry());

  user_cloud_policy_manager_ = policy::UserCloudPolicyManager::Create(
      path, schema_registry_service_->registry(),
      true /* force_immediate_policy_load */, io_task_runner,
      base::BindRepeating(&content::GetNetworkConnectionTracker));

  profile_policy_connector_ = policy::CreateAndInitProfilePolicyConnector(
      schema_registry_service_->registry(),
      static_cast<policy::ChromeBrowserPolicyConnector*>(
          browser_policy_connector),
      user_cloud_policy_manager_.get(),
      user_cloud_policy_manager_->core()->store(),
      true /* force_immediate_policy_load*/, nullptr /* user */);

  RegisterProfilePrefs(false /* is_signin_profile */,
                       chrome_feature_list_creator_->actual_locale(),
                       pref_registry_.get());

  mojo::PendingRemote<prefs::mojom::TrackedPreferenceValidationDelegate>
      pref_validation_delegate;
  // The preference tracking and protection is not required on Android.
  DCHECK(!ProfilePrefStoreManager::kPlatformSupportsPreferenceTracking);

  prefs_ = ::CreateProfilePrefService(
      pref_registry_, nullptr /* extension_pref_store */,
      profile_policy_connector_->policy_service(), browser_policy_connector,
      std::move(pref_validation_delegate), io_task_runner, key_.get(), path,
      false /* async_prefs*/);
}
#endif
