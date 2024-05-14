// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_filter/subresource_filter_history_observer.h"

#include "base/check.h"
#include "base/not_fatal_until.h"
#include "components/subresource_filter/content/browser/subresource_filter_content_settings_manager.h"
#include "url/gurl.h"

SubresourceFilterHistoryObserver::SubresourceFilterHistoryObserver(
    subresource_filter::SubresourceFilterContentSettingsManager*
        settings_manager,
    history::HistoryService* history_service)
    : settings_manager_(settings_manager) {
  CHECK(settings_manager_, base::NotFatalUntil::M129);
  CHECK(history_service, base::NotFatalUntil::M129);
  history_observation_.Observe(history_service);
}

SubresourceFilterHistoryObserver::~SubresourceFilterHistoryObserver() = default;

// Instructs |settings_manager_| to clear the relevant site metadata on URLs
// being deleted from history.
void SubresourceFilterHistoryObserver::OnHistoryDeletions(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  if (deletion_info.IsAllHistory()) {
    settings_manager_->ClearMetadataForAllSites();
    return;
  }

  for (const auto& entry : deletion_info.deleted_urls_origin_map()) {
    const GURL& origin = entry.first;
    int remaining_urls = entry.second.first;
    if (!origin.is_empty() && remaining_urls == 0)
      settings_manager_->ClearSiteMetadata(origin);
  }
}
