// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/local_data_container.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "components/browsing_data/core/features.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "net/cookies/canonical_cookie.h"

///////////////////////////////////////////////////////////////////////////////
// LocalDataContainer, public:

// static
std::unique_ptr<LocalDataContainer>
LocalDataContainer::CreateFromLocalSharedObjectsContainer(
    const browsing_data::LocalSharedObjectsContainer& shared_objects) {
  return std::make_unique<LocalDataContainer>(
      shared_objects.cookies(), shared_objects.local_storages(),
      shared_objects.session_storages(), /*quota_helper=*/nullptr,
      shared_objects.cache_storages());
}

// static
std::unique_ptr<LocalDataContainer>
LocalDataContainer::CreateFromStoragePartition(
    content::StoragePartition* storage_partition,
    browsing_data::CookieHelper::IsDeletionDisabledCallback
        is_cookie_deletion_disabled_callback) {
  return std::make_unique<LocalDataContainer>(
      base::FeatureList::IsEnabled(
          browsing_data::features::kDeprecateCookiesTreeModel)
          ? nullptr
          : base::MakeRefCounted<browsing_data::CookieHelper>(
                storage_partition, is_cookie_deletion_disabled_callback),
      /*local_storage_helper=*/nullptr,
      /*session_storage_helper=*/nullptr,
      /*quota_helper=*/nullptr,
      /*cache_storage_helper=*/nullptr);
}

LocalDataContainer::LocalDataContainer(
    scoped_refptr<browsing_data::CookieHelper> cookie_helper,
    scoped_refptr<browsing_data::LocalStorageHelper> local_storage_helper,
    scoped_refptr<browsing_data::LocalStorageHelper> session_storage_helper,
    scoped_refptr<BrowsingDataQuotaHelper> quota_helper,
    scoped_refptr<browsing_data::CacheStorageHelper> cache_storage_helper)
    : cookie_helper_(std::move(cookie_helper)),
      local_storage_helper_(std::move(local_storage_helper)),
      session_storage_helper_(std::move(session_storage_helper)),
      quota_helper_(std::move(quota_helper)),
      cache_storage_helper_(std::move(cache_storage_helper)) {}

LocalDataContainer::~LocalDataContainer() {}

void LocalDataContainer::Init(CookiesTreeModel* model) {
  DCHECK(!model_);
  model_ = model;

  int batches_started = 0;
  if (cookie_helper_.get()) {
    batches_started++;
    cookie_helper_->StartFetching(
        base::BindOnce(&LocalDataContainer::OnCookiesModelInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  if (local_storage_helper_.get()) {
    batches_started++;
    local_storage_helper_->StartFetching(
        base::BindOnce(&LocalDataContainer::OnLocalStorageModelInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  if (session_storage_helper_.get()) {
    batches_started++;
    session_storage_helper_->StartFetching(
        base::BindOnce(&LocalDataContainer::OnSessionStorageModelInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  if (quota_helper_.get()) {
    batches_started++;
    quota_helper_->StartFetching(
        base::BindOnce(&LocalDataContainer::OnQuotaModelInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  if (cache_storage_helper_.get()) {
    batches_started++;
    cache_storage_helper_->StartFetching(
        base::BindOnce(&LocalDataContainer::OnCacheStorageModelInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  // TODO(crbug.com/1271155): When `kDeprecateCookiesTreeModel` is enabled the
  // `LocalDataContainer` does not have any backends left to run asynchronously
  // which causes any added observers post model build to be skipped. Posting a
  // batch to UI thread to maintain async behaviour and allow time for observers
  // to be added to the CookiesTreeModel before it notifies build completion.
  // This is a temporary fix until this model could be deprecated and tests are
  // updated.
  if (base::FeatureList::IsEnabled(
          browsing_data::features::kDeprecateCookiesTreeModel) &&
      batches_started == 0) {
    batches_started++;
    auto scoped_notifier =
        std::make_unique<CookiesTreeModel::ScopedBatchUpdateNotifier>(
            model_.get(), model_->GetRoot());
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::DoNothingWithBoundArgs(std::move(scoped_notifier)));
  }

  // Don't reset batches, as some completions may have been reported
  // synchronously. As this function is called during model construction, there
  // can't have been any batches started outside this function.
  model_->SetBatchExpectation(batches_started, /*reset=*/false);
}

void LocalDataContainer::OnCookiesModelInfoLoaded(
    const net::CookieList& cookie_list) {
  cookie_list_.insert(cookie_list_.begin(),
                      cookie_list.begin(),
                      cookie_list.end());
  DCHECK(model_);
  model_->PopulateCookieInfo(this);
}

void LocalDataContainer::OnLocalStorageModelInfoLoaded(
    const LocalStorageInfoList& local_storage_info) {
  local_storage_info_list_ = local_storage_info;
  DCHECK(model_);
  model_->PopulateLocalStorageInfo(this);
}

void LocalDataContainer::OnSessionStorageModelInfoLoaded(
    const LocalStorageInfoList& session_storage_info) {
  session_storage_info_list_ = session_storage_info;
  DCHECK(model_);
  model_->PopulateSessionStorageInfo(this);
}

void LocalDataContainer::OnQuotaModelInfoLoaded(
    const QuotaInfoList& quota_info) {
  quota_info_list_ = quota_info;
  DCHECK(model_);
  model_->PopulateQuotaInfo(this);
}

void LocalDataContainer::OnCacheStorageModelInfoLoaded(
    const CacheStorageUsageInfoList& cache_storage_info) {
  cache_storage_info_list_ = cache_storage_info;
  DCHECK(model_);
  model_->PopulateCacheStorageUsageInfo(this);
}
