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

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
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
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/origin.h"

namespace {

constexpr char kManagedConfigurationDirectoryName[] = "Managed Configuration";
// Maximum configuration size is 5MB.
constexpr int kMaxConfigurationFileSize = 5 * 1024 * 1024;
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
      : api_(api), origin_(origin), store_path_(std::move(store_path)) {}

  ~OriginConfiguration() = default;
  OriginConfiguration(const OriginConfiguration&) = delete;
  OriginConfiguration& operator=(const OriginConfiguration&) = delete;

  void GetConfiguration(const std::vector<std::string>& keys,
                        ConfigurationCallback callback) {
    if (!store_) {
      std::move(callback).Run(std::nullopt);
      return;
    }
    store_.AsyncCall(&ManagedConfigurationStore::Get)
        .WithArgs(keys)
        .Then(std::move(callback));
  }

  void GetAllConfiguration(ConfigurationCallback callback) {
    EnsureStoreCreated();
    store_.AsyncCall(&ManagedConfigurationStore::GetAll)
        .Then(std::move(callback));
  }

  void SetConfigurationForTesting(base::DictValue configuration) {
    PostStoreConfiguration(std::move(configuration));
  }

  void UpdateConfiguration(const std::string& configuration_url,
                           const std::string& configuration_hash) {
    const std::string* last_hash_value =
        api_->profile_->GetPrefs()
            ->GetDict(prefs::kLastManagedConfigurationHashForOrigin)
            .FindString(GetOriginEncoded(origin_));

    // Nothing to be stored here, the hash value is the same.
    if (last_hash_value && *last_hash_value == configuration_hash) {
      return;
    }

    if (configuration_url.empty()) {
      ClearConfiguration();
      return;
    }

    // Check whether there is already a downloader.
    if (downloader_) {
      // If it downloads the same data already, do nothing.
      if (downloader_->hash() == configuration_hash) {
        return;
      }
      // Cancel it otherwise.
      downloader_.reset();
    }

    downloader_ =
        std::make_unique<ManagedConfigurationDownloader>(configuration_hash);
    downloader_->Fetch(
        configuration_url,
        base::BindOnce(&OriginConfiguration::DecodeData,
                       weak_ptr_factory_.GetWeakPtr(), configuration_hash));
  }

  void ClearConfiguration() {
    downloader_.reset();
    ScopedDictPrefUpdate update(api_->profile_->GetPrefs(),
                                prefs::kLastManagedConfigurationHashForOrigin);
    update->Remove(GetOriginEncoded(origin_));
    PostStoreConfiguration(base::DictValue());
  }

  void EnsureStoreCreated() {
    if (store_) {
      return;
    }
    // Create the store now, and serve the cached policy until the PolicyService
    // sends updated values.
    store_ = base::SequenceBound<ManagedConfigurationStore>(
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}),
        origin_, store_path_);
  }

  bool HasStore() const { return static_cast<bool>(store_); }

 private:
  void DecodeData(const std::string& url_hash,
                  std::optional<std::string> data) {
    downloader_.reset();
    if (!data) {
      return;
    }

    std::optional<base::DictValue> decoding_result =
        base::JSONReader::ReadDict(*data, base::JSON_PARSE_RFC);
    if (!decoding_result.has_value()) {
      VLOG(1) << "Could not fetch managed configuration for app with origin = "
              << origin_.Serialize();
      PostStoreConfiguration(base::DictValue());
      return;
    }
    ScopedDictPrefUpdate update(api_->profile_->GetPrefs(),
                                prefs::kLastManagedConfigurationHashForOrigin);
    update->Set(GetOriginEncoded(origin_), url_hash);

    // We need to transform each value into a string.
    base::DictValue result_dict;
    for (auto item : *decoding_result) {
      std::string result;
      JSONStringValueSerializer serializer(&result);
      serializer.Serialize(item.second);
      result_dict.SetByDottedPath(item.first, result);
    }

    PostStoreConfiguration(std::move(result_dict));
  }

  void PostStoreConfiguration(base::DictValue configuration) {
    EnsureStoreCreated();
    store_.AsyncCall(&ManagedConfigurationStore::SetCurrentPolicy)
        .WithArgs(std::move(configuration))
        .Then(
            base::BindOnce(&OriginConfiguration::OnStoreConfigurationCompleted,
                           weak_ptr_factory_.GetWeakPtr()));
  }

  void OnStoreConfigurationCompleted(
      base::expected<bool, std::monostate> changed) {
    api_->InformObserversIfConfigurationChanged(origin_,
                                                changed.value_or(false));
  }

  const raw_ptr<ManagedConfigurationAPI> api_;
  const url::Origin origin_;
  const base::FilePath store_path_;

  base::SequenceBound<ManagedConfigurationStore> store_;
  std::unique_ptr<ManagedConfigurationDownloader> downloader_;

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
  if (!origins_.contains(origin)) {
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
    GetOrCreateOriginConfiguration(origin).EnsureStoreCreated();
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

void ManagedConfigurationAPI::OnConfigurationPolicyChanged() {
  const base::ListValue& managed_configurations =
      profile_->GetPrefs()->GetList(prefs::kManagedConfigurationPerOrigin);

  std::set<url::Origin> current_origins;

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

    if (!configuration_url || !configuration_hash) {
      continue;
    }
    GetOrCreateOriginConfiguration(origin).UpdateConfiguration(
        *configuration_url, *configuration_hash);
  }

  // Clean configurations for origins that had their entry removed.
  for (auto& [origin, origin_configuration] : origins_) {
    if (!current_origins.contains(origin) && origin_configuration->HasStore()) {
      origin_configuration->ClearConfiguration();
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
