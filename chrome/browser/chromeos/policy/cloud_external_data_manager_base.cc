// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/cloud_external_data_manager_base.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/cloud_external_data_store.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/external_policy_data_fetcher.h"
#include "components/policy/core/common/cloud/external_policy_data_updater.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_map.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace policy {

namespace {

// Fetch data for at most two external data references at the same time.
const int kMaxParallelFetches = 2;

// Allows policies to reference |g_max_external_data_size_for_testing| bytes of
// external data even if no |max_size| was specified in policy_templates.json.
int g_max_external_data_size_for_testing = 0;

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

  // Allows downloaded external data to be cached in |external_data_store|.
  // Ownership of the store is taken. The store can be destroyed by calling
  // SetExternalDataStore(std::unique_ptr<CloudExternalDataStore>()).
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

  // Called by the |updater_| when the external |data| referenced by |policy|
  // has been successfully downloaded and verified to match |hash|.
  bool OnDownloadSuccess(const std::string& policy,
                         const std::string& hash,
                         const std::string& data);

  // Retrieves the external data referenced by |policy| and invokes |callback|
  // with the result. If |policy| does not reference any external data, the
  // |callback| is invoked with a NULL pointer. Otherwise, the |callback| is
  // invoked with the referenced data once it has been successfully retrieved.
  // If retrieval is temporarily impossible (e.g. the data is not cached yet and
  // there is no network connectivity), the |callback| will be invoked when the
  // temporary hindrance is resolved. If retrieval is permanently impossible
  // (e.g. |policy| references data that does not exist on the server), the
  // |callback| will never be invoked.
  // If the data for |policy| is not cached yet, only one download is started,
  // even if this method is invoked multiple times. The |callback|s passed are
  // enqueued and all invoked once the data has been successfully retrieved.
  void Fetch(const std::string& policy,
             ExternalDataFetcher::FetchCallback callback);

  // Try to download and cache all external data referenced by |metadata_|.
  void FetchAll();

 private:
  // List of callbacks to invoke when the attempt to retrieve external data
  // referenced by a policy completes successfully or fails permanently.
  typedef std::vector<ExternalDataFetcher::FetchCallback> FetchCallbackList;

  // Map from policy names to the lists of callbacks defined above.
  typedef std::map<std::string, FetchCallbackList> FetchCallbackMap;

  // Looks up the maximum size that the data referenced by |policy| can have.
  size_t GetMaxExternalDataSize(const std::string& policy) const;

  // Invokes |callback| via the |callback_task_runner_|, passing |data| and
  // |file_path| as parameters.
  void RunCallback(ExternalDataFetcher::FetchCallback callback,
                   std::unique_ptr<std::string> data,
                   const base::FilePath& file_path) const;

  // Tells the |updater_| to download the external data referenced by |policy|.
  // If Connect() was not called yet and no |updater_| exists, does nothing.
  void StartDownload(const std::string& policy);

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

  DISALLOW_COPY_AND_ASSIGN(Backend);
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
  if (metadata_set_ && external_data_store_)
    external_data_store_->Prune(metadata_);
}

void CloudExternalDataManagerBase::Backend::Connect(
    std::unique_ptr<ExternalPolicyDataFetcher> external_policy_data_fetcher) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!updater_);
  updater_.reset(new ExternalPolicyDataUpdater(
      task_runner_, std::move(external_policy_data_fetcher),
      kMaxParallelFetches));
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

  if (external_data_store_)
    external_data_store_->Prune(metadata_);

  for (FetchCallbackMap::iterator it = pending_downloads_.begin();
       it != pending_downloads_.end(); ) {
    const std::string policy = it->first;
    Metadata::const_iterator metadata = metadata_.find(policy);
    if (metadata == metadata_.end()) {
      // |policy| no longer references external data.
      if (updater_) {
        // Cancel the external data download.
        updater_->CancelExternalDataFetch(policy);
      }
      for (ExternalDataFetcher::FetchCallback& callback : it->second) {
        // Invoke all callbacks for |policy|, indicating permanent failure.
        RunCallback(std::move(callback), std::unique_ptr<std::string>(),
                    base::FilePath());
      }
      pending_downloads_.erase(it++);
      continue;
    }

    if (updater_ && metadata->second != old_metadata[policy]) {
      // |policy| still references external data but the reference has changed.
      // Cancel the external data download and start a new one.
      updater_->CancelExternalDataFetch(policy);
      StartDownload(policy);
    }
    ++it;
  }
}

bool CloudExternalDataManagerBase::Backend::OnDownloadSuccess(
    const std::string& policy,
    const std::string& hash,
    const std::string& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(metadata_.find(policy) != metadata_.end());
  DCHECK_EQ(hash, metadata_[policy].hash);
  base::FilePath file_path;
  if (external_data_store_)
    file_path = external_data_store_->Store(policy, hash, data);

  FetchCallbackList& pending_callbacks = pending_downloads_[policy];
  for (ExternalDataFetcher::FetchCallback& callback : pending_callbacks) {
    RunCallback(std::move(callback), std::make_unique<std::string>(data),
                file_path);
  }
  pending_downloads_.erase(policy);
  return true;
}

void CloudExternalDataManagerBase::Backend::Fetch(
    const std::string& policy,
    ExternalDataFetcher::FetchCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Metadata::const_iterator metadata = metadata_.find(policy);
  if (metadata == metadata_.end()) {
    // If |policy| does not reference any external data, indicate permanent
    // failure.
    RunCallback(std::move(callback), std::unique_ptr<std::string>(),
                base::FilePath());
    return;
  }

  if (pending_downloads_.find(policy) != pending_downloads_.end()) {
    // If a download of the external data referenced by |policy| has already
    // been requested, add |callback| to the list of callbacks for |policy| and
    // return.
    pending_downloads_[policy].push_back(std::move(callback));
    return;
  }

  std::unique_ptr<std::string> data(new std::string);
  if (external_data_store_) {
    base::FilePath file_path =
        external_data_store_->Load(policy, metadata->second.hash,
                                   GetMaxExternalDataSize(policy), data.get());
    if (!file_path.empty()) {
      // If the external data referenced by |policy| exists in the cache and
      // matches the expected hash, pass it to the callback.
      RunCallback(std::move(callback), std::move(data), file_path);
      return;
    }
  }

  // Request a download of the the external data referenced by |policy| and
  // initialize the list of callbacks by adding |callback|.
  pending_downloads_[policy].push_back(std::move(callback));
  StartDownload(policy);
}

void CloudExternalDataManagerBase::Backend::FetchAll() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Loop through all external data references.
  for (const auto& it : metadata_) {
    const std::string& policy = it.first;
    std::unique_ptr<std::string> data(new std::string);
    if (pending_downloads_.find(policy) != pending_downloads_.end() ||
        (external_data_store_ &&
         !external_data_store_
              ->Load(policy, it.second.hash, GetMaxExternalDataSize(policy),
                     data.get())
              .empty())) {
      // If a download of the external data referenced by |policy| has already
      // been requested or the data exists in the cache and matches the expected
      // hash, there is nothing to be done.
      continue;
    }
    // Request a download of the the external data referenced by |policy| and
    // initialize the list of callbacks to an empty list.
    pending_downloads_[policy];
    StartDownload(policy);
  }
}

size_t CloudExternalDataManagerBase::Backend::GetMaxExternalDataSize(
    const std::string& policy) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (g_max_external_data_size_for_testing)
    return g_max_external_data_size_for_testing;

  // Look up the maximum size that the data referenced by |policy| can have in
  // get_policy_details, which is constructed from the information in
  // policy_templates.json, allowing the maximum data size to be specified as
  // part of the policy definition.
  const PolicyDetails* details = get_policy_details_.Run(policy);
  if (details)
    return details->max_external_data_size;
  NOTREACHED();
  return 0;
}

void CloudExternalDataManagerBase::Backend::RunCallback(
    ExternalDataFetcher::FetchCallback callback,
    std::unique_ptr<std::string> data,
    const base::FilePath& file_path) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  callback_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), base::Passed(&data), file_path));
}

void CloudExternalDataManagerBase::Backend::StartDownload(
    const std::string& policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pending_downloads_.find(policy) != pending_downloads_.end());
  if (!updater_)
    return;

  const MetadataEntry& metadata = metadata_[policy];
  updater_->FetchExternalData(
      policy,
      ExternalPolicyDataUpdater::Request(metadata.url,
                                         metadata.hash,
                                         GetMaxExternalDataSize(policy)),
      base::Bind(&CloudExternalDataManagerBase::Backend::OnDownloadSuccess,
                 base::Unretained(this),
                 policy,
                 metadata.hash));
}

CloudExternalDataManagerBase::CloudExternalDataManagerBase(
    const GetChromePolicyDetailsCallback& get_policy_details,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner)
    : backend_task_runner_(std::move(backend_task_runner)),
      backend_(new Backend(get_policy_details,
                           backend_task_runner_,
                           base::ThreadTaskRunnerHandle::Get())) {}

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
                                base::Passed(&external_data_store)));
}

void CloudExternalDataManagerBase::SetPolicyStore(
    CloudPolicyStore* policy_store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CloudExternalDataManager::SetPolicyStore(policy_store);
  if (policy_store_ && policy_store_->is_initialized())
    OnPolicyStoreLoaded();
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
    const base::DictionaryValue* dict = NULL;
    std::string url;
    std::string hex_hash;
    std::string hash;
    if (it.second.value && it.second.value->GetAsDictionary(&dict) &&
        dict->GetStringWithoutPathExpansion("url", &url) &&
        dict->GetStringWithoutPathExpansion("hash", &hex_hash) &&
        !url.empty() && !hex_hash.empty() &&
        base::HexStringToString(hex_hash, &hash)) {
      // Add the external data reference to |metadata| if it is valid (URL and
      // hash are not empty, hash can be decoded as a hex string).
      (*metadata)[it.first] = MetadataEntry(url, hash);
    }
  }

  backend_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Backend::OnMetadataUpdated,
                                base::Unretained(backend_.get()),
                                base::Passed(&metadata)));
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
    ExternalDataFetcher::FetchCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Backend::Fetch, base::Unretained(backend_.get()), policy,
                     std::move(callback)));
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
