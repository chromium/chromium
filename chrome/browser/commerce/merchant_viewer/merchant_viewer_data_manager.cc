// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/commerce/merchant_viewer/merchant_viewer_data_manager.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "chrome/browser/persisted_state_db/session_proto_db_factory.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/session_proto_db/session_proto_db.h"
#include "content/public/browser/android/browser_context_handle.h"

MerchantViewerDataManager::MerchantViewerDataManager(
    content::BrowserContext* browser_context)
    : proto_db_(SessionProtoDBFactory<MerchantSignalProto>::GetInstance()
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!HasValidDB()) {
    return;
  }

  int deleted_items_count = 0;
  for (const auto& item : data) {
    MerchantSignalProto proto = std::move(item.second);
    base::Time time_created = base::Time::FromSecondsSinceUnixEpoch(
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!HasValidDB()) {
    return;
  }

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
  if (success && data.size() == 1 && HasValidDB()) {
    MerchantSignalProto proto = std::move(data.at(0).second);
    proto_db_->DeleteOneEntry(proto.key(), base::BindOnce(&OnUpdateCallback));
  }
}

void MerchantViewerDataManager::DeleteMerchantViewerDataForOrigins(
    const base::flat_set<GURL>& deleted_origins) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!HasValidDB()) {
    return;
  }

  auto force_delete_all_merchants =
      commerce::kDeleteAllMerchantsOnClearBrowsingHistory.Get();
  if (force_delete_all_merchants) {
    ClearAllMerchants();
    LOCAL_HISTOGRAM_BOOLEAN(
        "MerchantViewer.DataManager.ForceClearMerchantsForOrigins", true);
  } else {
    auto deleted_hostnames =
        base::MakeFlatSet<std::string>(deleted_origins, {}, &GURL::host);
    LOG(ERROR) << "Clearing " << deleted_hostnames.size() << " merchants.";
    proto_db_->LoadAllEntries(base::BindOnce(
        &MerchantViewerDataManager::OnLoadAllEntriesForOriginsCallback,
        weak_ptr_factory_.GetWeakPtr(), std::move(deleted_hostnames)));
  }
}

void MerchantViewerDataManager::DeleteMerchantViewerDataForTimeRange(
    base::Time created_after,
    base::Time created_before) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!HasValidDB()) {
    return;
  }

  bool force_delete_all_merchants =
      commerce::kDeleteAllMerchantsOnClearBrowsingHistory.Get();
  if (force_delete_all_merchants) {
    ClearAllMerchants();
    LOCAL_HISTOGRAM_BOOLEAN(
        "MerchantViewer.DataManager.ForceClearMerchantsForTimeRange", true);
  } else {
    proto_db_->LoadAllEntries(base::BindOnce(
        &MerchantViewerDataManager::OnLoadAllEntriesForTimeRangeCallback,
        weak_ptr_factory_.GetWeakPtr(), created_after, created_before));
  }
}

void MerchantViewerDataManager::ClearAllMerchants() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (HasValidDB()) {
    proto_db_->DeleteAllContent(base::BindOnce(&OnUpdateCallback));
  }
}

bool MerchantViewerDataManager::HasValidDB() {
  return proto_db_ != nullptr;
}

SessionProtoDB<merchant_signal_db::MerchantSignalContentProto>*
MerchantViewerDataManager::GetDB() {
  return proto_db_;
}
