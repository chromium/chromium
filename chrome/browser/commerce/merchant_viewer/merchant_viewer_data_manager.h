// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMMERCE_MERCHANT_VIEWER_MERCHANT_VIEWER_DATA_MANAGER_H_
#define CHROME_BROWSER_COMMERCE_MERCHANT_VIEWER_MERCHANT_VIEWER_DATA_MANAGER_H_

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/session_proto_db/session_proto_db.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace merchant_signal_db {
class MerchantSignalContentProto;
}  // namespace merchant_signal_db

template <typename T>
class SessionProtoDB;

// Abstracts merchant viewer local data management.
class MerchantViewerDataManager : public KeyedService {
 public:
  using MerchantSignalProto = merchant_signal_db::MerchantSignalContentProto;
  using MerchantSignals =
      std::vector<SessionProtoDB<MerchantSignalProto>::KeyAndValue>;

  explicit MerchantViewerDataManager(content::BrowserContext* browser_context);
  ~MerchantViewerDataManager() override;

  void DeleteMerchantViewerDataForOrigins(
      const base::flat_set<GURL>& deleted_origins);
  void DeleteMerchantViewerDataForTimeRange(base::Time created_after,
                                            base::Time created_before);

  SessionProtoDB<merchant_signal_db::MerchantSignalContentProto>* GetDB();

 private:
  void OnLoadCallbackSingleEntry(bool success, MerchantSignals data);
  void OnLoadAllEntriesForOriginsCallback(
      const base::flat_set<std::string>& deleted_hostnames,
      bool success,
      MerchantSignals data);
  void OnLoadAllEntriesForTimeRangeCallback(base::Time begin,
                                            base::Time end,
                                            bool success,
                                            MerchantSignals data);

  void ClearAllMerchants();
  bool HasValidDB();
  SEQUENCE_CHECKER(sequence_checker_);
  raw_ptr<SessionProtoDB<merchant_signal_db::MerchantSignalContentProto>>
      proto_db_;
  base::WeakPtrFactory<MerchantViewerDataManager> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_COMMERCE_MERCHANT_VIEWER_MERCHANT_VIEWER_DATA_MANAGER_H_
