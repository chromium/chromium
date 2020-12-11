// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENGAGEMENT_IMPORTANT_SITES_USAGE_COUNTER_H_
#define CHROME_BROWSER_ENGAGEMENT_IMPORTANT_SITES_USAGE_COUNTER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/sequenced_task_runner_helpers.h"
#include "chrome/browser/engagement/important_sites_util.h"
#include "content/public/browser/storage_usage_info.h"
#include "storage/browser/quota/quota_manager.h"

namespace content {
class DOMStorageContext;
}

namespace storage {
class QuotaManager;
}

// A helper class for important storage that retrieves the localstorage and
// quota usage for each domain in |ImportantDomainInfo|, populates
// |ImportantDomainInfo::usage| in the |sites| entries and return the result via
// |callback|.
class ImportantSitesUsageCounter {
 public:
  using UsageCallback = base::OnceCallback<void(
      std::vector<ImportantSitesUtil::ImportantDomainInfo> sites)>;

  // Populates the ImportantDomainInfo::usage field of each site with the amount
  // of localstorage and quota bytes used. |callback| is asynchronously invoked
  // on the UI thread with the |sites| vector (populated with usage) as an
  // argument.
  static void GetUsage(
      std::vector<ImportantSitesUtil::ImportantDomainInfo> sites,
      storage::QuotaManager* quota_manager,
      content::DOMStorageContext* dom_storage_context,
      UsageCallback callback);

 private:
  friend class base::DeleteHelper<ImportantSitesUsageCounter>;

  ImportantSitesUsageCounter(
      std::vector<ImportantSitesUtil::ImportantDomainInfo> sites,
      storage::QuotaManager* quota_manager,
      content::DOMStorageContext* dom_storage_context,
      UsageCallback callback);
  ~ImportantSitesUsageCounter();

  void RunAndDestroySelfWhenFinished();

  void GetQuotaUsageOnIOThread();

  void ReceiveQuotaUsageOnIOThread(std::vector<storage::UsageInfo> usage_infos);

  void ReceiveQuotaUsage(std::vector<storage::UsageInfo> usage_infos);

  void ReceiveLocalStorageUsage(
      const std::vector<content::StorageUsageInfo>& storage_infos);

  // Look up the corresponding ImportantDomainInfo for |url| and increase its
  // usage by |size|.
  void IncrementUsage(const std::string& domain, int64_t size);

  void Done();

  UsageCallback callback_;
  std::vector<ImportantSitesUtil::ImportantDomainInfo> sites_;
  storage::QuotaManager* quota_manager_;
  content::DOMStorageContext* dom_storage_context_;
  int tasks_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ImportantSitesUsageCounter);
};

#endif  // CHROME_BROWSER_ENGAGEMENT_IMPORTANT_SITES_USAGE_COUNTER_H_
