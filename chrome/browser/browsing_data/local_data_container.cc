// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/local_data_container.h"

#include <utility>

#include "base/functional/bind.h"
#include "components/browsing_data/core/features.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "net/cookies/canonical_cookie.h"

///////////////////////////////////////////////////////////////////////////////
// LocalDataContainer, public:

// static
std::unique_ptr<LocalDataContainer>
LocalDataContainer::CreateFromStoragePartition(
    content::StoragePartition* storage_partition,
    browsing_data::CookieHelper::IsDeletionDisabledCallback
        is_cookie_deletion_disabled_callback) {
  return std::make_unique<LocalDataContainer>(
      /*cookie_helper=*/nullptr,
      /*local_storage_helper=*/nullptr,
      /*session_storage_helper=*/nullptr,
      /*quota_helper=*/nullptr);
}

LocalDataContainer::LocalDataContainer(
    scoped_refptr<browsing_data::CookieHelper> cookie_helper,
    scoped_refptr<browsing_data::LocalStorageHelper> local_storage_helper,
    scoped_refptr<browsing_data::LocalStorageHelper> session_storage_helper,
    scoped_refptr<BrowsingDataQuotaHelper> quota_helper)
    : cookie_helper_(std::move(cookie_helper)),
      local_storage_helper_(std::move(local_storage_helper)),
      session_storage_helper_(std::move(session_storage_helper)),
      quota_helper_(std::move(quota_helper)) {}

LocalDataContainer::~LocalDataContainer() {}

void LocalDataContainer::Init() {
  if (cookie_helper_.get()) {
    cookie_helper_->StartFetching(
        base::BindOnce(&LocalDataContainer::OnCookiesModelInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  if (local_storage_helper_.get()) {
    local_storage_helper_->StartFetching(
        base::BindOnce(&LocalDataContainer::OnLocalStorageModelInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  if (session_storage_helper_.get()) {
    session_storage_helper_->StartFetching(
        base::BindOnce(&LocalDataContainer::OnSessionStorageModelInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  if (quota_helper_.get()) {
    quota_helper_->StartFetching(
        base::BindOnce(&LocalDataContainer::OnQuotaModelInfoLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void LocalDataContainer::OnCookiesModelInfoLoaded(
    const net::CookieList& cookie_list) {
  cookie_list_.insert(cookie_list_.begin(),
                      cookie_list.begin(),
                      cookie_list.end());
}

void LocalDataContainer::OnLocalStorageModelInfoLoaded(
    const LocalStorageInfoList& local_storage_info) {
  local_storage_info_list_ = local_storage_info;
}

void LocalDataContainer::OnSessionStorageModelInfoLoaded(
    const LocalStorageInfoList& session_storage_info) {
  session_storage_info_list_ = session_storage_info;
}

void LocalDataContainer::OnQuotaModelInfoLoaded(
    const QuotaInfoList& quota_info) {
  quota_info_list_ = quota_info;
}
