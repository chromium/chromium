// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_flash_lso_helper.h"

#include <stdint.h>

#include <limits>
#include <map>

#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "chrome/browser/pepper_flash_settings_manager.h"

namespace {

class BrowsingDataFlashLSOHelperImpl
    : public BrowsingDataFlashLSOHelper,
      public PepperFlashSettingsManager::Client {
 public:
  explicit BrowsingDataFlashLSOHelperImpl(
      content::BrowserContext* browser_context);

  // BrowsingDataFlashLSOHelper implementation:
  void StartFetching(GetSitesWithFlashDataCallback callback) override;
  void DeleteFlashLSOsForSite(const std::string& site,
                              base::OnceClosure callback) override;

  // PepperFlashSettingsManager::Client overrides:
  void OnGetSitesWithDataCompleted(
      uint32_t request_id,
      const std::vector<std::string>& sites) override;
  void OnClearSiteDataCompleted(uint32_t request_id, bool success) override;

 private:
  struct DeleteFlashLSOTask {
    DeleteFlashLSOTask() {}
    DeleteFlashLSOTask(const std::string& site, base::OnceClosure callback)
        : site(site), callback(std::move(callback)) {}

    std::string site;
    base::OnceClosure callback;
  };

  ~BrowsingDataFlashLSOHelperImpl() override;

  // Asynchronously fetches and deletes data and calls us back.
  PepperFlashSettingsManager settings_manager_;

  // Identifies the request to fetch site data.
  uint32_t get_sites_with_data_request_id_;

  // Contains the pending requests to clear site data. The key is the request
  // ID, the value is the site for which to clear data and the callback to be
  // called upon completion.
  std::map<uint32_t, DeleteFlashLSOTask> clear_site_data_ids_;

  // Called when we have fetched the list of sites.
  GetSitesWithFlashDataCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataFlashLSOHelperImpl);
};

BrowsingDataFlashLSOHelperImpl::BrowsingDataFlashLSOHelperImpl(
    content::BrowserContext* browser_context)
    : settings_manager_(this, browser_context),
      get_sites_with_data_request_id_(0u) {
}

BrowsingDataFlashLSOHelperImpl::~BrowsingDataFlashLSOHelperImpl() {
}

void BrowsingDataFlashLSOHelperImpl::StartFetching(
    GetSitesWithFlashDataCallback callback) {
  DCHECK(callback_.is_null());
  callback_ = std::move(callback);
  get_sites_with_data_request_id_ = settings_manager_.GetSitesWithData();
}

void BrowsingDataFlashLSOHelperImpl::DeleteFlashLSOsForSite(
    const std::string& site,
    base::OnceClosure callback) {
  const uint64_t kClearAllData = 0;
  uint32_t id = settings_manager_.ClearSiteData(
      site, kClearAllData, std::numeric_limits<uint64_t>::max());
  clear_site_data_ids_[id] = DeleteFlashLSOTask(site, std::move(callback));
}

void BrowsingDataFlashLSOHelperImpl::OnGetSitesWithDataCompleted(
    uint32_t request_id,
    const std::vector<std::string>& sites) {
  DCHECK_EQ(get_sites_with_data_request_id_, request_id);
  std::move(callback_).Run(sites);
}

void BrowsingDataFlashLSOHelperImpl::OnClearSiteDataCompleted(
    uint32_t request_id,
    bool success) {
  auto entry = clear_site_data_ids_.find(request_id);
  DCHECK(entry != clear_site_data_ids_.end());
  LOG_IF(ERROR, !success) << "Couldn't clear Flash LSO data for "
                          << entry->second.site;
  if (!entry->second.callback.is_null())
    std::move(entry->second.callback).Run();
  clear_site_data_ids_.erase(entry);
}

}  // namespace

// static
BrowsingDataFlashLSOHelper* BrowsingDataFlashLSOHelper::Create(
    content::BrowserContext* browser_context) {
  return new BrowsingDataFlashLSOHelperImpl(browser_context);
}
