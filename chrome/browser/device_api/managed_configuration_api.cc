// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_api/managed_configuration_api.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/device_api/managed_configuration_store.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "net/base/backoff_entry.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/origin.h"

namespace features {
BASE_FEATURE(kManagedConfigurationRecoverFromFailedFetches,
             base::FEATURE_ENABLED_BY_DEFAULT);
}  // namespace features

namespace {

bool IsRecoveryEnabled() {
  return base::FeatureList::IsEnabled(
      features::kManagedConfigurationRecoverFromFailedFetches);
}

constexpr char kManagedConfigurationDirectoryName[] = "Managed Configuration";
// Maximum configuration size is 5MB.
constexpr int kMaxConfigurationFileSize = 5 * 1024 * 1024;
// Per-attempt download timeout. SimpleURLLoader restarts it for each internal
// retry and it covers the whole body, so it is sized for a 5 MB configuration
// at ~0.7 Mbit/s. Without it, a stalled server blocks all recovery.
constexpr base::TimeDelta kDownloadAttemptTimeout = base::Seconds(60);
constexpr net::BackoffEntry::Policy kRetryBackoffPolicy = {
    0,          // Number of initial errors to ignore.
    1000,       // Initial delay in ms (1 second).
    2.0,        // Multiply factor.
    0.2,        // Jitter factor.
    5 * 60000,  // Maximum backoff in ms (5 minutes).
    -1,         // Entry lifetime.
    false,      // Always use initial delay.
};

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("managed_configuration_loader",
                                        R"(
        semantics {
          sender: "Web App Managed Configuration Downloader"
          description:
            "Fetches JSON managed configuration from the DM server, for the "
            "origins configured in the ManagedConfigurationPerOrigin policy. "
            "This enables the navigatior.device.getManagedConfiguration(keys) "
            "JavaScript function, to obtain the values of this configuration "
            "that correspond to provided keys. This configuration can only be "
            "set for force-installed web applications, which are defined by "
            "WebAppInstallForceList policy."
          trigger:
            "Changes in the ManagedConfigurationPerOrigin policy."
          data: "No user data."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled in settings, but it will only be "
            "sent if the device administrator sets up a configuration policy "
            "for a particular origin.",
          chrome_policy {
            ManagedConfigurationPerOrigin {
              ManagedConfigurationPerOrigin: "[]"
            }
          }
        })");

// Converts url::Origin into the key that can be used for filenames/dictionary
// keys.
std::string GetOriginEncoded(const url::Origin& origin) {
  return base::HexEncode(origin.Serialize());
}

class ManagedConfigurationDownloader {
 public:
  explicit ManagedConfigurationDownloader(const std::string& data_hash)
      : data_hash_(data_hash) {}
  ~ManagedConfigurationDownloader() = default;
  ManagedConfigurationDownloader(const ManagedConfigurationDownloader&) =
      delete;
  ManagedConfigurationDownloader& operator=(
      const ManagedConfigurationDownloader&) = delete;

  void Fetch(const std::string& data_url,
             base::OnceCallback<void(std::optional<std::string>)> callback) {
    // URLLoaders should be created at UI thread.
    CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);
    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url = GURL(data_url);
    resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

    simple_loader_ = network::SimpleURLLoader::Create(
        std::move(resource_request), kTrafficAnnotation);

    simple_loader_->SetRetryOptions(
        /* max_retries=*/3,
        network::SimpleURLLoader::RETRY_ON_5XX |
            network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
    if (IsRecoveryEnabled()) {
      simple_loader_->SetTimeoutDuration(kDownloadAttemptTimeout);
    }

    SystemNetworkContextManager* system_network_context_manager =
        g_browser_process->system_network_context_manager();
    network::mojom::URLLoaderFactory* loader_factory =
        system_network_context_manager->GetURLLoaderFactory();

    simple_loader_->DownloadToString(loader_factory, std::move(callback),
                                     kMaxConfigurationFileSize);
  }

  const std::string& hash() const { return data_hash_; }

 private:
  std::unique_ptr<network::SimpleURLLoader> simple_loader_;
  const std::string data_hash_;
};

}  // namespace

const char ManagedConfigurationAPI::kOriginKey[] = "origin";
const char ManagedConfigurationAPI::kManagedConfigurationUrlKey[] =
    "managed_configuration_url";
const char ManagedConfigurationAPI::kManagedConfigurationHashKey[] =
    "managed_configuration_hash";

class ManagedConfigurationAPI::OriginConfiguration {
 public:
  using ConfigurationCallback =
      base::OnceCallback<void(std::optional<base::DictValue>)>;

  OriginConfiguration(ManagedConfigurationAPI* api,
                      const url::Origin& origin,
                      base::FilePath store_path)
      : api_(api),
        origin_(origin),
        store_path_(std::move(store_path)),
        retry_policy_(MakeRetryPolicy(*api)) {}

  ~OriginConfiguration() = default;

  OriginConfiguration(const OriginConfiguration&) = delete;
  OriginConfiguration& operator=(const OriginConfiguration&) = delete;

  void GetConfiguration(const std::vector<std::string>& keys,
                        ConfigurationCallback callback) {
    if (!IsRecoveryEnabled()) {
      if (!store_) {
        std::move(callback).Run(std::nullopt);
        return;
      }
      store_.AsyncCall(&ManagedConfigurationStore::Get)
          .WithArgs(keys)
          .Then(std::move(callback));
      return;
    }

    EnsureSynced();
    EnsureStoreCreated();
    store_.AsyncCall(&ManagedConfigurationStore::Get)
        .WithArgs(keys)
        .Then(std::move(callback));
  }

  void GetAllConfiguration(ConfigurationCallback callback) {
    if (!IsRecoveryEnabled()) {
      EnsureStoreCreated();
      store_.AsyncCall(&ManagedConfigurationStore::GetAll)
          .Then(std::move(callback));
      return;
    }

    EnsureSynced();
    EnsureStoreCreated();
    store_.AsyncCall(&ManagedConfigurationStore::GetAll)
        .Then(std::move(callback));
  }

  void SetConfigurationForTesting(base::DictValue configuration) {
    PostStoreConfiguration(/*hash_to_commit=*/std::nullopt,
                           std::move(configuration));
  }

  void UpdateConfiguration(const std::string& configuration_url,
                           const std::string& configuration_hash) {
    DesiredConfiguration desired{configuration_url, configuration_hash};
    if (desired_ != desired) {
      retry_timer_.Stop();
      retry_backoff_.Reset();
      retry_count_ = 0;
    }
    desired_ = std::move(desired);
    SyncStoreWithDesiredConfiguration();
  }

  void ClearConfiguration() {
    desired_.reset();
    downloader_.reset();
    retry_timer_.Stop();
    retry_backoff_.Reset();
    retry_count_ = 0;
    ScopedDictPrefUpdate update(api_->profile_->GetPrefs(),
                                prefs::kLastManagedConfigurationHashForOrigin);
    update->Remove(GetOriginEncoded(origin_));
    PostStoreConfiguration(/*hash_to_commit=*/std::nullopt, base::DictValue());
  }

  void EnsureSynced() {
    EnsureStoreCreated();
    if (!IsRecoveryEnabled()) {
      return;
    }
    if (!desired_) {
      return;
    }
    SyncStoreWithDesiredConfiguration();
  }

  void OnNetworkConnected() {
    if (!IsRecoveryEnabled()) {
      return;
    }
    retry_timer_.Stop();
    // Reset backoff to fast-track the pending retry on reconnection, while
    // keeping `retry_count_` intact so total retries for `desired_` remain
    // accurate.
    retry_backoff_.Reset();
    EnsureSynced();
  }

  bool HasStore() const { return static_cast<bool>(store_); }
  bool IsFetchInProgress() const { return downloader_ != nullptr; }

 private:
  // Configuration URL and hash requested by policy.
  struct DesiredConfiguration {
    std::string url;
    std::string hash;
    friend bool operator==(const DesiredConfiguration&,
                           const DesiredConfiguration&) = default;
  };

  static net::BackoffEntry::Policy MakeRetryPolicy(
      const ManagedConfigurationAPI& api) {
    net::BackoffEntry::Policy policy = kRetryBackoffPolicy;
    if (api.retry_initial_delay_for_testing_) {
      policy.initial_delay_ms =
          api.retry_initial_delay_for_testing_->InMilliseconds();
      policy.multiply_factor = api.retry_multiply_factor_for_testing_;
    }
    return policy;
  }

  void ScheduleRetry(RetryReason reason) {
    if (!IsRecoveryEnabled()) {
      return;
    }
    ++retry_count_;
    base::UmaHistogramEnumeration(kRetryReasonHistogram, reason);
    retry_backoff_.InformOfRequest(false);
    retry_timer_.Start(FROM_HERE, retry_backoff_.GetTimeUntilRelease(),
                       base::BindOnce(&OriginConfiguration::EnsureSynced,
                                      weak_ptr_factory_.GetWeakPtr()));
  }

  void EnsureStoreCreated() {
    if (store_) {
      return;
    }
    // Create the store now, and serve the cached policy until the PolicyService
    // sends updated values. With kManagedConfigurationRecoverFromFailedFetches,
    // `SKIP_ON_SHUTDOWN` is safe because
    // `kLastManagedConfigurationHashForOrigin` is only committed in
    // `OnStoreConfigurationCompleted()` after `SetCurrentPolicy()` finishes;
    // if shutdown skips an in-flight write, the next session startup will see
    // the uncommitted hash and re-sync.
    scoped_refptr<base::SequencedTaskRunner> task_runner =
        api_->store_task_runner_for_testing_;
    if (!task_runner) {
      task_runner = base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
    }
    store_ = base::SequenceBound<ManagedConfigurationStore>(
        std::move(task_runner), origin_, store_path_);
  }

  void SyncStoreWithDesiredConfiguration() {
    CHECK(desired_);
    const std::string* last_hash_value =
        api_->profile_->GetPrefs()
            ->GetDict(prefs::kLastManagedConfigurationHashForOrigin)
            .FindString(GetOriginEncoded(origin_));

    // Nothing to be stored here, the hash value is the same.
    if (last_hash_value && *last_hash_value == desired_->hash) {
      if (IsRecoveryEnabled()) {
        downloader_.reset();
        retry_timer_.Stop();
        retry_backoff_.Reset();
        retry_count_ = 0;
      }
      return;
    }

    if (desired_->url.empty()) {
      ClearConfiguration();
      return;
    }

    // Check whether there is already a downloader.
    if (downloader_) {
      // If it downloads the same data already, do nothing.
      if (downloader_->hash() == desired_->hash) {
        return;
      }
      // Cancel it otherwise.
      downloader_.reset();
    }

    // A queued write will commit exactly the desired hash (or schedule a retry
    // if it fails), so downloading it again would be redundant.
    if (IsRecoveryEnabled() && in_flight_write_hash_ == desired_->hash) {
      return;
    }

    // Do not preempt an active backoff timer unless it was explicitly stopped
    // by a policy change or network reconnection.
    if (IsRecoveryEnabled() && retry_timer_.IsRunning()) {
      return;
    }

    downloader_ =
        std::make_unique<ManagedConfigurationDownloader>(desired_->hash);
    downloader_->Fetch(
        desired_->url,
        base::BindOnce(&OriginConfiguration::DecodeData,
                       weak_ptr_factory_.GetWeakPtr(), desired_->hash));
  }

  void DecodeData(const std::string& url_hash,
                  std::optional<std::string> data) {
    downloader_.reset();
    if (!data) {
      ScheduleRetry(RetryReason::kDownloadFailed);
      return;
    }

    std::optional<base::DictValue> decoding_result =
        base::JSONReader::ReadDict(*data, base::JSON_PARSE_RFC);
    if (!decoding_result.has_value()) {
      VLOG(1) << "Could not fetch managed configuration for app with origin = "
              << origin_.Serialize();
      PostStoreConfiguration(/*hash_to_commit=*/std::nullopt,
                             base::DictValue());
      ScheduleRetry(RetryReason::kInvalidJson);
      return;
    }
    retry_timer_.Stop();

    if (!IsRecoveryEnabled()) {
      ScopedDictPrefUpdate update(
          api_->profile_->GetPrefs(),
          prefs::kLastManagedConfigurationHashForOrigin);
      update->Set(GetOriginEncoded(origin_), url_hash);
    }

    // We need to transform each value into a string.
    base::DictValue result_dict;
    for (auto item : *decoding_result) {
      std::string result;
      JSONStringValueSerializer serializer(&result);
      serializer.Serialize(item.second);
      if (IsRecoveryEnabled()) {
        result_dict.Set(item.first, result);
      } else {
        result_dict.SetByDottedPath(item.first, result);
      }
    }

    PostStoreConfiguration(url_hash, std::move(result_dict));
  }

  // `hash_to_commit` is committed once this write succeeds and no later write
  // is queued, or std::nullopt for writes that must never be committed (clears
  // and test writes). An empty hash is a valid policy hash.
  void PostStoreConfiguration(std::optional<std::string> hash_to_commit,
                              base::DictValue configuration) {
    EnsureStoreCreated();
    if (IsRecoveryEnabled()) {
      // Drop the committed hash before the store diverges from it; it is
      // re-committed in OnStoreConfigurationCompleted() once the latest write
      // succeeded. Otherwise a superseded or failed write could leave a hash
      // that does not match the store (e.g. policy A -> B -> A while B's write
      // is queued).
      ScopedDictPrefUpdate update(
          api_->profile_->GetPrefs(),
          prefs::kLastManagedConfigurationHashForOrigin);
      update->Remove(GetOriginEncoded(origin_));
    }
    ++in_flight_store_writes_;
    // Writes complete in order, so the latest queued write determines what the
    // store will contain.
    in_flight_write_hash_ = hash_to_commit;
    store_.AsyncCall(&ManagedConfigurationStore::SetCurrentPolicy)
        .WithArgs(std::move(configuration))
        .Then(base::BindOnce(
            &OriginConfiguration::OnStoreConfigurationCompleted,
            weak_ptr_factory_.GetWeakPtr(), std::move(hash_to_commit)));
  }

  void OnStoreConfigurationCompleted(
      std::optional<std::string> hash_to_commit,
      base::expected<bool, std::monostate> changed) {
    CHECK_GT(in_flight_store_writes_, 0);
    if (--in_flight_store_writes_ == 0) {
      in_flight_write_hash_.reset();
    }
    if (!IsRecoveryEnabled()) {
      api_->InformObserversIfConfigurationChanged(origin_,
                                                  changed.value_or(false));
      return;
    }
    // Writes complete in order, so a write with later writes queued behind it
    // does not determine what the store contains. Only the latest write may
    // commit its hash or schedule a retry.
    const bool is_desired_write = in_flight_store_writes_ == 0 && desired_ &&
                                  hash_to_commit == desired_->hash;
    if (!changed.has_value()) {
      if (is_desired_write) {
        ScheduleRetry(RetryReason::kStoreWriteFailed);
      }
      return;
    }
    if (is_desired_write) {
      ScopedDictPrefUpdate update(
          api_->profile_->GetPrefs(),
          prefs::kLastManagedConfigurationHashForOrigin);
      update->Set(GetOriginEncoded(origin_), *hash_to_commit);
      base::UmaHistogramCounts1000(kRetryCountUntilSuccessHistogram,
                                   retry_count_);
      // Only a durably committed configuration counts as success; resetting
      // earlier would keep the backoff from growing while writes keep failing.
      retry_backoff_.Reset();
      retry_count_ = 0;
    }
    api_->InformObserversIfConfigurationChanged(origin_, *changed);
  }

  const raw_ptr<ManagedConfigurationAPI> api_;
  const url::Origin origin_;
  const base::FilePath store_path_;
  const net::BackoffEntry::Policy retry_policy_;

  // Configuration requested by policy, or std::nullopt if there is none (e.g.
  // after ClearConfiguration()). Empty strings are valid policy values.
  std::optional<DesiredConfiguration> desired_;
  base::SequenceBound<ManagedConfigurationStore> store_;
  std::unique_ptr<ManagedConfigurationDownloader> downloader_;
  net::BackoffEntry retry_backoff_{&retry_policy_};
  base::OneShotTimer retry_timer_;
  // Number of retries scheduled for the current `desired_` configuration.
  // Tracked separately from `retry_backoff_.failure_count()` because network
  // reconnection resets `retry_backoff_` to retry immediately.
  int retry_count_ = 0;
  int in_flight_store_writes_ = 0;
  // Hash the most recently queued store write will commit, while any write is
  // in flight. std::nullopt for clears and test writes.
  std::optional<std::string> in_flight_write_hash_;

  base::WeakPtrFactory<OriginConfiguration> weak_ptr_factory_{this};
};

ManagedConfigurationAPI::ManagedConfigurationAPI(Profile* profile)
    : profile_(profile),
      stores_path_(
          profile->GetPath().AppendASCII(kManagedConfigurationDirectoryName)) {
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(profile_->GetPrefs());
  pref_change_registrar_->Add(
      prefs::kManagedConfigurationPerOrigin,
      base::BindRepeating(
          &ManagedConfigurationAPI::OnConfigurationPolicyChanged,
          weak_ptr_factory_.GetWeakPtr()));
  if (IsRecoveryEnabled()) {
    if (auto* tracker = content::GetNetworkConnectionTracker()) {
      network_connection_observation_.Observe(tracker);
    }
  }
  OnConfigurationPolicyChanged();
}

ManagedConfigurationAPI::~ManagedConfigurationAPI() = default;

// static
void ManagedConfigurationAPI::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kManagedConfigurationPerOrigin);
  registry->RegisterDictionaryPref(
      prefs::kLastManagedConfigurationHashForOrigin);
}

void ManagedConfigurationAPI::GetOriginPolicyConfiguration(
    const url::Origin& origin,
    const std::vector<std::string>& keys,
    base::OnceCallback<void(std::optional<base::DictValue>)> callback) {
  if (!CanHaveManagedStore(origin)) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  if (!IsRecoveryEnabled() && !origins_.contains(origin)) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  GetOrCreateOriginConfiguration(origin).GetConfiguration(keys,
                                                          std::move(callback));
}

void ManagedConfigurationAPI::GetOriginPolicyConfiguration(
    const url::Origin& origin,
    base::OnceCallback<void(std::optional<base::DictValue>)> callback) {
  if (!CanHaveManagedStore(origin)) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  GetOrCreateOriginConfiguration(origin).GetAllConfiguration(
      std::move(callback));
}

void ManagedConfigurationAPI::AddObserver(Observer* observer) {
  url::Origin origin = observer->GetOrigin();
  if (CanHaveManagedStore(origin)) {
    GetOrCreateOriginConfiguration(origin).EnsureSynced();
    observers_[origin].AddObserver(observer);
  } else {
    unmanaged_observers_.insert(observer);
  }
}

void ManagedConfigurationAPI::RemoveObserver(Observer* observer) {
  auto iter = unmanaged_observers_.find(observer);
  if (iter != unmanaged_observers_.end()) {
    unmanaged_observers_.erase(iter);
    return;
  }

  observers_[observer->GetOrigin()].RemoveObserver(observer);
}

void ManagedConfigurationAPI::AddObserver(AnyOriginObserver* observer) {
  any_origin_observers_.AddObserver(observer);
}

void ManagedConfigurationAPI::RemoveObserver(AnyOriginObserver* observer) {
  any_origin_observers_.RemoveObserver(observer);
}

bool ManagedConfigurationAPI::CanHaveManagedStore(const url::Origin& origin) {
  return managed_origins_.contains(origin);
}

const std::set<url::Origin>& ManagedConfigurationAPI::GetManagedOrigins()
    const {
  return managed_origins_;
}

void ManagedConfigurationAPI::SetConfigurationForTesting(
    const url::Origin& origin,
    base::DictValue configuration) {
  managed_origins_.insert(origin);
  GetOrCreateOriginConfiguration(origin).SetConfigurationForTesting(
      std::move(configuration));
}

void ManagedConfigurationAPI::SetRetryBackoffForTesting(
    base::TimeDelta initial_delay,
    double multiply_factor) {
  retry_initial_delay_for_testing_ = initial_delay;
  retry_multiply_factor_for_testing_ = multiply_factor;
}

void ManagedConfigurationAPI::SetStoreTaskRunnerForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  store_task_runner_for_testing_ = std::move(task_runner);
}

bool ManagedConfigurationAPI::IsFetchInProgressForTesting(
    const url::Origin& origin) const {
  auto it = origins_.find(origin);
  return it != origins_.end() && it->second->IsFetchInProgress();
}

void ManagedConfigurationAPI::OnConnectionChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  if (!IsRecoveryEnabled() ||
      type == net::NetworkChangeNotifier::CONNECTION_NONE) {
    return;
  }
  for (const auto& origin : managed_origins_) {
    GetOrCreateOriginConfiguration(origin).OnNetworkConnected();
  }
}

void ManagedConfigurationAPI::OnConfigurationPolicyChanged() {
  const base::ListValue& managed_configurations =
      profile_->GetPrefs()->GetList(prefs::kManagedConfigurationPerOrigin);

  std::set<url::Origin> current_origins;
  std::set<std::string> current_encoded_origins;

  for (const auto& entry : managed_configurations) {
    const auto& entry_dict = entry.GetDict();
    const std::string* url = entry_dict.FindString(kOriginKey);
    if (!url) {
      continue;
    }
    const url::Origin origin = url::Origin::Create(GURL(*url));
    if (origin.opaque()) {
      continue;
    }

    const std::string* configuration_url =
        entry_dict.FindString(kManagedConfigurationUrlKey);
    const std::string* configuration_hash =
        entry_dict.FindString(kManagedConfigurationHashKey);
    current_origins.insert(origin);
    current_encoded_origins.insert(GetOriginEncoded(origin));

    if (!configuration_url || !configuration_hash) {
      if (IsRecoveryEnabled()) {
        if (auto it = origins_.find(origin); it != origins_.end()) {
          it->second->ClearConfiguration();
        }
      }
      continue;
    }
    GetOrCreateOriginConfiguration(origin).UpdateConfiguration(
        *configuration_url, *configuration_hash);
  }

  // Clean configurations for origins that had their entry removed.
  for (auto& [origin, origin_configuration] : origins_) {
    if (!current_origins.contains(origin)) {
      if (IsRecoveryEnabled() || origin_configuration->HasStore()) {
        origin_configuration->ClearConfiguration();
      }
    }
  }

  if (IsRecoveryEnabled()) {
    // Also remove persisted hashes for origins that were removed from policy
    // across browser sessions (and therefore are not in `origins_`).
    ScopedDictPrefUpdate update(profile_->GetPrefs(),
                                prefs::kLastManagedConfigurationHashForOrigin);
    std::vector<std::string> stale_keys;
    for (auto [encoded_origin, _] : *update) {
      if (!current_encoded_origins.contains(encoded_origin)) {
        stale_keys.push_back(encoded_origin);
      }
    }
    for (const std::string& key : stale_keys) {
      update->Remove(key);
    }
  }

  managed_origins_.swap(current_origins);
  PromoteObservers();
}

ManagedConfigurationAPI::OriginConfiguration&
ManagedConfigurationAPI::GetOrCreateOriginConfiguration(
    const url::Origin& origin) {
  auto& entry = origins_[origin];
  if (!entry) {
    entry = std::make_unique<OriginConfiguration>(this, origin,
                                                  GetStoreLocation(origin));
  }
  return *entry;
}

base::FilePath ManagedConfigurationAPI::GetStoreLocation(
    const url::Origin& origin) {
  return stores_path_.AppendASCII(GetOriginEncoded(origin));
}

void ManagedConfigurationAPI::InformObserversIfConfigurationChanged(
    const url::Origin& origin,
    bool has_changed) {
  if (!has_changed) {
    return;
  }

  for (auto& observer : any_origin_observers_) {
    observer.OnAnyManagedConfigurationChanged(origin);
  }

  if (observers_.contains(origin)) {
    for (auto& observer : observers_[origin]) {
      observer.OnManagedConfigurationChanged();
    }
  }
}

void ManagedConfigurationAPI::PromoteObservers() {
  for (auto it = unmanaged_observers_.begin();
       it != unmanaged_observers_.end();) {
    if (CanHaveManagedStore((*it)->GetOrigin())) {
      auto* observer = (*it).get();
      it = unmanaged_observers_.erase(it);
      AddObserver(observer);
    } else {
      ++it;
    }
  }
}
