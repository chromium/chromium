// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/external_data/cloud_external_data_manager_base.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/callback_list.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "components/policy/core/common/cloud/cloud_external_data_store.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/external_policy_data_fetcher.h"
#include "components/policy/core/common/cloud/external_policy_data_updater.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace policy {

namespace {

// Fetch data for at most two external data references at the same time.
const int kMaxParallelFetches = 2;

// Allows policies to reference |g_max_external_data_size_for_testing| bytes of
// external data even if no |max_size| was specified in policy_templates.json.
int g_max_external_data_size_for_testing = 0;

// Keys for 'Value::Dict' objects
const char kUrlKey[] = "url";
const char kHashKey[] = "hash";
const char kCustomIconKey[] = "custom_icon";
}  // namespace

// Backend for the CloudExternalDataManagerBase that handles all data download,
// verification, caching and retrieval.
class CloudExternalDataManagerBase::Backend {
 public:
  // |get_policy_details| is used to determine the maximum size that the
  // data referenced by each policy can have. This class can be instantiated on
  // any thread but from then on, may be accessed via the |task_runner_| only.
  // All FetchCallbacks will be invoked via |callback_task_runner|.
  Backend(const GetChromePolicyDetailsCallback& get_policy_details,
          scoped_refptr<base::SequencedTaskRunner> task_runner,
          scoped_refptr<base::SequencedTaskRunner> callback_task_runner);

  Backend(const Backend&) = delete;
  Backend& operator=(const Backend&) = delete;

  // Allows downloaded external data to be cached in |external_data_store|.
  // Ownership of the store is taken. The store can be destroyed by calling
  // SetExternalDataStore(nullptr).
  void SetExternalDataStore(
      std::unique_ptr<CloudExternalDataStore> external_data_store);

  // Allows downloading of external data via the |external_policy_data_fetcher|.
  void Connect(
      std::unique_ptr<ExternalPolicyDataFetcher> external_policy_data_fetcher);

  // Prevents further external data downloads and aborts any downloads currently
  // in progress
  void Disconnect();

  // Called when the external data references that this backend is responsible
  // for change. |metadata| maps from policy names to the metadata specifying
  // the external data that each of the policies references.
  void OnMetadataUpdated(std::unique_ptr<Metadata> metadata);

  // Called by the |updater_| when the external |data| referenced by |key|
  // has been successfully downloaded and verified to match |hash|.
  bool OnDownloadSuccess(const MetadataKey& key,
                         const std::string& hash,
                         const std::string& data);

  // Retrieves the external data referenced by |key| and invokes |callback|
  // with the result. If |key| does not reference any external data, the
  // |callback| is invoked with a NULL pointer. Otherwise, the |callback| is
  // invoked with the referenced data once it has been successfully retrieved.
  // If retrieval is temporarily impossible (e.g. the data is not cached yet and
  // there is no network connectivity), the |callback| will be invoked when the
  // temporary hindrance is resolved. If retrieval is permanently impossible
  // (e.g. |key| references data that does not exist on the server), the
  // |callback| will never be invoked.
  // If the data for |key| is not cached yet, only one download is started,
  // even if this method is invoked multiple times. The |callback|s passed are
  // enqueued and all invoked once the data has been successfully retrieved.
  void Fetch(const MetadataKey& key,
             ExternalDataFetcher::FetchCallback callback);

  // Try to download and cache all external data referenced by |metadata_|.
  void FetchAll();

 private:
  // List of callbacks to invoke when the attempt to retrieve external data
  // referenced by a policy completes successfully or fails permanently.
  using FetchCallbackList =
      base::OnceCallbackList<void(const std::string*, const base::FilePath&)>;

  // Map from policy names to the lists of callbacks defined above.
  using FetchCallbackMap = std::map<MetadataKey, FetchCallbackList>;

  // Looks up the maximum size that the data referenced by |key.policy| can
  // have.
  size_t GetMaxExternalDataSize(const MetadataKey& key) const;

  // Invokes |callback| via the |callback_task_runner_|, passing |data| and
  // |file_path| as parameters.
  void RunCallback(ExternalDataFetcher::FetchCallback callback,
                   std::unique_ptr<std::string> data,
                   const base::FilePath& file_path) const;

  // Tells the |updater_| to download the external data referenced by |key|.
  // If Connect() was not called yet and no |updater_| exists, does nothing.
  void StartDownload(const MetadataKey& key);

  void PruneDataStore();

  // Used to determine the maximum size that the data referenced by each policy
  // can have.
  GetChromePolicyDetailsCallback get_policy_details_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_refptr<base::SequencedTaskRunner> callback_task_runner_;

  // Contains the policies for which a download of the referenced external data
  // has been requested. Each policy is mapped to a list of callbacks to invoke
  // when the download completes successfully or fails permanently. If no
  // callback needs to be invoked (because the download was requested via
  // FetchAll()), a map entry will still exist but the list of callbacks it maps
  // to will be empty.
  FetchCallbackMap pending_downloads_;

  // Indicates that OnMetadataUpdated() has been called at least once and the
  // contents of |metadata_| is initialized.
  bool metadata_set_;

  // Maps from policy names to the metadata specifying the external data that
  // each of the policies references.
  Metadata metadata_;

  // Used to cache external data referenced by policies.
  std::unique_ptr<CloudExternalDataStore> external_data_store_;

  // Used to download external data referenced by policies.
  std::unique_ptr<ExternalPolicyDataUpdater> updater_;

  SEQUENCE_CHECKER(sequence_checker_);
};

CloudExternalDataManagerBase::Backend::Backend(
    const GetChromePolicyDetailsCallback& get_policy_details,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner)
    : get_policy_details_(get_policy_details),
      task_runner_(task_runner),
      callback_task_runner_(callback_task_runner),
      metadata_set_(false) {
  // This class is allowed to be instantiated on any thread.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void CloudExternalDataManagerBase::Backend::SetExternalDataStore(
    std::unique_ptr<CloudExternalDataStore> external_data_store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  external_data_store_ = std::move(external_data_store);
  if (metadata_set_)
    PruneDataStore();
}

void CloudExternalDataManagerBase::Backend::Connect(
    std::unique_ptr<ExternalPolicyDataFetcher> external_policy_data_fetcher) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!updater_);
  updater_ = std::make_unique<ExternalPolicyDataUpdater>(
      task_runner_, std::move(external_policy_data_fetcher),
      kMaxParallelFetches);
  for (const auto& it : pending_downloads_)
    StartDownload(it.first);
}

void CloudExternalDataManagerBase::Backend::Disconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  updater_.reset();
}

void CloudExternalDataManagerBase::Backend::OnMetadataUpdated(
    std::unique_ptr<Metadata> metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  metadata_set_ = true;
  Metadata old_metadata;
  metadata_.swap(old_metadata);
  if (metadata)
    metadata_.swap(*metadata);

  PruneDataStore();

  for (FetchCallbackMap::iterator it = pending_downloads_.begin();
       it != pending_downloads_.end();) {
    const MetadataKey key = it->first;
    Metadata::const_iterator metadata_iter = metadata_.find(key);
    if (metadata_iter == metadata_.end()) {
      // |policy| no longer references external data.
      if (updater_) {
        // Cancel the external data download.
        updater_->CancelExternalDataFetch(key.ToString());
      }
      // Invoke all callbacks for |key|, indicating permanent failure.
      it->second.Notify(nullptr, base::FilePath());
      pending_downloads_.erase(it++);
      continue;
    }

    if (updater_ && metadata_iter->second != old_metadata[key]) {
      // |policy| still references external data but the reference has changed.
      // Cancel the external data download and start a new one.
      updater_->CancelExternalDataFetch(key.ToString());
      StartDownload(key);
    }
    ++it;
  }
}

bool CloudExternalDataManagerBase::Backend::OnDownloadSuccess(
    const MetadataKey& key,
    const std::string& hash,
    const std::string& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(metadata_.find(key) != metadata_.end());
  DCHECK_EQ(hash, metadata_[key].hash);
  base::FilePath file_path;
  if (external_data_store_)
    file_path = external_data_store_->Store(key.ToString(), hash, data);

  pending_downloads_[key].Notify(&data, file_path);
  pending_downloads_.erase(key);
  return true;
}

void CloudExternalDataManagerBase::Backend::Fetch(
    const MetadataKey& key,
    ExternalDataFetcher::FetchCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Metadata::const_iterator metadata = metadata_.find(key);
  if (metadata == metadata_.end()) {
    // If |policy| does not reference any external data, indicate permanent
    // failure.
    RunCallback(std::move(callback), nullptr, base::FilePath());
    return;
  }

  const bool has_pending_download = base::Contains(pending_downloads_, key);
  if (!has_pending_download && external_data_store_) {
    auto data = std::make_unique<std::string>();
    const base::FilePath file_path =
        external_data_store_->Load(key.ToString(), metadata->second.hash,
                                   GetMaxExternalDataSize(key), data.get());
    if (!file_path.empty()) {
      // If the external data referenced by |policy| exists in the cache and
      // matches the expected hash, pass it to the callback.
      RunCallback(std::move(callback), std::move(data), file_path);
      return;
    }
  }

  // Callback lists cannot hold callbacks that take move-only args, since
  // Notify()ing such a list would move the arg into the first callback, leaving
  // it null or unspecified for remaining callbacks.  Instead, adapt the
  // provided callbacks to accept a raw pointer, which can be copied, and then
  // wrap in a separate scoping object for each callback.
  pending_downloads_[key].AddUnsafe(base::BindOnce(
      [](const CloudExternalDataManagerBase::Backend* backend,
         ExternalDataFetcher::FetchCallback callback, const std::string* data,
         const base::FilePath& file_path) {
        backend->RunCallback(
            std::move(callback),
            data ? std::make_unique<std::string>(*data) : nullptr, file_path);
      },
      base::Unretained(this), std::move(callback)));
  if (!has_pending_download)
    StartDownload(key);
}

void CloudExternalDataManagerBase::Backend::FetchAll() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Loop through all external data references.
  for (const auto& it : metadata_) {
    const MetadataKey& key = it.first;
    std::unique_ptr<std::string> data(new std::string);
    if (base::Contains(pending_downloads_, key) ||
        (external_data_store_ &&
         !external_data_store_
              ->Load(key.ToString(), it.second.hash,
                     GetMaxExternalDataSize(key), data.get())
              .empty())) {
      // If a download of the external data referenced by |key| has already
      // been requested or the data exists in the cache and matches the expected
      // hash, there is nothing to be done.
      continue;
    }
    // Initialize the list of callbacks referenced by |key| to an empty list.
    pending_downloads_[key];
    // Request a download of the external data referenced by |key|.
    StartDownload(key);
  }
}

size_t CloudExternalDataManagerBase::Backend::GetMaxExternalDataSize(
    const MetadataKey& key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (g_max_external_data_size_for_testing)
    return g_max_external_data_size_for_testing;

  // Look up the maximum size that the data referenced by |policy| can have in
  // get_policy_details, which is constructed from the information in
  // policy_templates.json, allowing the maximum data size to be specified as
  // part of the policy definition.
  const PolicyDetails* details = get_policy_details_.Run(key.policy);
  if (details)
    return details->max_external_data_size;
  NOTREACHED_IN_MIGRATION();
  return 0;
}

void CloudExternalDataManagerBase::Backend::RunCallback(
    ExternalDataFetcher::FetchCallback callback,
    std::unique_ptr<std::string> data,
    const base::FilePath& file_path) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  callback_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(data), file_path));
}

void CloudExternalDataManagerBase::Backend::StartDownload(
    const MetadataKey& key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::Contains(pending_downloads_, key));
  if (!updater_)
    return;

  const MetadataEntry& metadata = metadata_[key];
  updater_->FetchExternalData(
      key.ToString(),
      ExternalPolicyDataUpdater::Request(metadata.url, metadata.hash,
                                         GetMaxExternalDataSize(key)),
      base::BindRepeating(
          &CloudExternalDataManagerBase::Backend::OnDownloadSuccess,
          base::Unretained(this), key, metadata.hash));
}

void CloudExternalDataManagerBase::Backend::PruneDataStore() {
  if (!external_data_store_)
    return;

  // Extract the list of (key, hash) pairs from the Metadata map to tell the
  // store which data should be kept.
  CloudExternalDataStore::PruningData key_hash_pairs;
  base::ranges::transform(metadata_, std::back_inserter(key_hash_pairs),
                          [](const std::pair<MetadataKey, MetadataEntry>& p) {
                            return make_pair(p.first.ToString(), p.second.hash);
                          });
  external_data_store_->Prune(key_hash_pairs);
}

CloudExternalDataManagerBase::CloudExternalDataManagerBase(
    const GetChromePolicyDetailsCallback& get_policy_details,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner)
    : backend_task_runner_(std::move(backend_task_runner)),
      backend_(new Backend(get_policy_details,
                           backend_task_runner_,
                           base::SingleThreadTaskRunner::GetCurrentDefault())) {
}

CloudExternalDataManagerBase::~CloudExternalDataManagerBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Backend* backend_to_delete = backend_.release();
  if (!backend_task_runner_->DeleteSoon(FROM_HERE, backend_to_delete)) {
    // If the task runner is no longer running, it's safe to just delete the
    // object, since no further events will be delivered by external data
    // manager.
    delete backend_to_delete;
  }
}

void CloudExternalDataManagerBase::SetExternalDataStore(
    std::unique_ptr<CloudExternalDataStore> external_data_store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Backend::SetExternalDataStore,
                                base::Unretained(backend_.get()),
                                std::move(external_data_store)));
}

void CloudExternalDataManagerBase::SetPolicyStore(
    CloudPolicyStore* policy_store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CloudExternalDataManager::SetPolicyStore(policy_store);
  if (policy_store_ && policy_store_->is_initialized())
    OnPolicyStoreLoaded();
}

// Extract a url and hash from |value_dict|, and put them into |metadata|.
void AddMetadataFromValue(CloudExternalDataManagerBase::Metadata* metadata,
                          const std::string& policy_name,
                          const std::string& field_name,
                          const base::Value::Dict& value_dict) {
  const std::string* url = value_dict.FindString(kUrlKey);
  const std::string* hex_hash = value_dict.FindString(kHashKey);
  std::string hash;
  if (url && hex_hash && !url->empty() && !hex_hash->empty() &&
      base::HexStringToString(*hex_hash, &hash)) {
    // Add the external data reference to |metadata| if it is valid (URL and
    // hash are not empty, hash can be decoded as a hex string).
    CloudExternalDataManagerBase::MetadataKey key(policy_name, field_name);
    CloudExternalDataManagerBase::MetadataEntry entry(*url, hash);
    (*metadata)[key] = entry;
  }
}

void CloudExternalDataManagerBase::OnPolicyStoreLoaded() {
  // Collect all external data references made by policies in |policy_store_|
  // and pass them to the |backend_|.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<Metadata> metadata(new Metadata);
  const PolicyMap& policy_map = policy_store_->policy_map();
  for (const auto& it : policy_map) {
    if (!it.second.external_data_fetcher) {
      // Skip policies that do not reference external data.
      continue;
    }
    if (it.first != key::kWebAppInstallForceList) {
      if (it.second.value(base::Value::Type::DICT)) {
        AddMetadataFromValue(
            metadata.get(), it.first, std::string(),
            it.second.value(base::Value::Type::DICT)->GetDict());
      }
      continue;
    }
    if (it.second.value(base::Value::Type::LIST)) {
      for (const auto& app :
           it.second.value(base::Value::Type::LIST)->GetList()) {
        if (app.is_dict()) {
          const base::Value::Dict& dict = app.GetDict();
          const base::Value::Dict* const icon = dict.FindDict(kCustomIconKey);
          const std::string* const url = dict.FindString(kUrlKey);
          if (icon && url) {
            AddMetadataFromValue(metadata.get(), it.first, *url, *icon);
          }
        }
      }
    }
  }

  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Backend::OnMetadataUpdated,
                     base::Unretained(backend_.get()), std::move(metadata)));
}

void CloudExternalDataManagerBase::Connect(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Backend::Connect, base::Unretained(backend_.get()),
                     std::make_unique<ExternalPolicyDataFetcher>(
                         std::move(url_loader_factory), backend_task_runner_)));
}

void CloudExternalDataManagerBase::Disconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Backend::Disconnect, base::Unretained(backend_.get())));
}

void CloudExternalDataManagerBase::Fetch(
    const std::string& policy,
    const std::string& field_name,
    ExternalDataFetcher::FetchCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Backend::Fetch, base::Unretained(backend_.get()),
                     MetadataKey(policy, field_name), std::move(callback)));
}

// static
void CloudExternalDataManagerBase::SetMaxExternalDataSizeForTesting(
    int max_size) {
  g_max_external_data_size_for_testing = max_size;
}

void CloudExternalDataManagerBase::FetchAll() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Backend::FetchAll, base::Unretained(backend_.get())));
}

}  // namespace policy
