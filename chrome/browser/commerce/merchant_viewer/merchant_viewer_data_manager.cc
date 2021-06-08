// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/commerce/merchant_viewer/merchant_viewer_data_manager.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "chrome/browser/persisted_state_db/profile_proto_db.h"
#include "chrome/browser/persisted_state_db/profile_proto_db_factory.h"
#include "components/embedder_support/android/browser_context/browser_context_handle.h"

MerchantViewerDataManager::MerchantViewerDataManager(
    content::BrowserContext* browser_context)
    : proto_db_(ProfileProtoDBFactory<MerchantSignalProto>::GetInstance()
                    ->GetForProfile(browser_context)) {}

MerchantViewerDataManager::~MerchantViewerDataManager() = default;

void OnUpdateCallback(bool success) {
  DCHECK(success) << "There was an error modifying MerchantSignalDB";
}

void MerchantViewerDataManager::OnLoadAllEntriesForTimeRangeCallback(
    base::Time begin,
    base::Time end,
    bool success,
    MerchantSignals data) {
  int deleted_items_count = 0;

  for (const auto& item : data) {
    MerchantSignalProto proto = std::move(item.second);
    base::Time time_created = base::Time::FromDoubleT(
        proto.trust_signals_message_displayed_timestamp());
    if (time_created >= begin && time_created <= end) {
      proto_db_->DeleteOneEntry(proto.key(), base::BindOnce(&OnUpdateCallback));
      deleted_items_count++;
    }
  }

  LOCAL_HISTOGRAM_COUNTS_100(
      "MerchantViewer.DataManager.DeleteMerchantViewerDataForTimeRange",
      deleted_items_count);
}

void MerchantViewerDataManager::OnLoadAllEntriesForOriginsCallback(
    const base::flat_set<std::string>& deleted_hostnames,
    bool success,
    MerchantSignals data) {
  int deleted_items_count = 0;

  for (const auto& item : data) {
    MerchantSignalProto proto = std::move(item.second);

    if (deleted_hostnames.contains(proto.key())) {
      proto_db_->DeleteOneEntry(proto.key(), base::BindOnce(&OnUpdateCallback));
      deleted_items_count++;
    }
  }

  LOCAL_HISTOGRAM_COUNTS_100(
      "MerchantViewer.DataManager.DeleteMerchantViewerDataForOrigins",
      deleted_items_count);
}

void MerchantViewerDataManager::OnLoadCallbackSingleEntry(
    bool success,
    MerchantSignals data) {
  if (success && data.size() == 1) {
    MerchantSignalProto proto = std::move(data.at(0).second);
    proto_db_->DeleteOneEntry(proto.key(), base::BindOnce(&OnUpdateCallback));
  }
}

void MerchantViewerDataManager::DeleteMerchantViewerDataForOrigins(
    const base::flat_set<GURL>& deleted_origins) {
  std::vector<std::string> deleted_hostnames;

  std::transform(deleted_origins.begin(), deleted_origins.end(),
                 std::back_inserter(deleted_hostnames),
                 [](const auto& item) { return item.host(); });

  proto_db_->LoadAllEntries(base::BindOnce(
      &MerchantViewerDataManager::OnLoadAllEntriesForOriginsCallback,
      weak_ptr_factory_.GetWeakPtr(),
      base::flat_set<std::string>(std::move(deleted_hostnames))));
}

void MerchantViewerDataManager::DeleteMerchantViewerDataForTimeRange(
    base::Time created_after,
    base::Time created_before) {
  proto_db_->LoadAllEntries(base::BindOnce(
      &MerchantViewerDataManager::OnLoadAllEntriesForTimeRangeCallback,
      weak_ptr_factory_.GetWeakPtr(), created_after, created_before));
}

ProfileProtoDB<merchant_signal_db::MerchantSignalContentProto>*
MerchantViewerDataManager::GetDB() {
  return proto_db_;
}
