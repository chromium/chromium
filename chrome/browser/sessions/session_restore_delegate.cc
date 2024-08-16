// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/sessions/session_restore_delegate.h"

#include <stddef.h>

#include <utility>

#include "base/metrics/field_trial.h"
#include "chrome/browser/sessions/session_restore_stats_collector.h"
#include "chrome/browser/sessions/tab_loader.h"
#include "chrome/common/url_constants.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/policies/background_tab_loading_policy.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "content/public/browser/web_contents.h"

namespace {

bool IsInternalPage(const GURL& url) {
  // There are many chrome:// UI URLs, but only look for the ones that users
  // are likely to have open. Most of the benefit is from the NTP URL.
  const char* const kReloadableUrlPrefixes[] = {
      chrome::kChromeUIDownloadsURL,
      chrome::kChromeUIHistoryURL,
      chrome::kChromeUINewTabURL,
      chrome::kChromeUISettingsURL,
  };
  // Prefix-match against the table above. Use strncmp to avoid allocating
  // memory to convert the URL prefix constants into std::strings.
  for (size_t i = 0; i < std::size(kReloadableUrlPrefixes); ++i) {
    if (!strncmp(url.spec().c_str(), kReloadableUrlPrefixes[i],
                 strlen(kReloadableUrlPrefixes[i])))
      return true;
  }
  return false;
}

}  // namespace

SessionRestoreDelegate::RestoredTab::RestoredTab(
    content::WebContents* contents,
    bool is_active,
    bool is_app,
    bool is_pinned,
    const std::optional<tab_groups::TabGroupId>& group)
    : contents_(contents->GetWeakPtr()),
      is_active_(is_active),
      is_app_(is_app),
      is_internal_page_(IsInternalPage(contents->GetLastCommittedURL())),
      is_pinned_(is_pinned),
      group_(group) {}

SessionRestoreDelegate::RestoredTab::RestoredTab(const RestoredTab&) = default;

SessionRestoreDelegate::RestoredTab&
SessionRestoreDelegate::RestoredTab::operator=(const RestoredTab&) = default;

SessionRestoreDelegate::RestoredTab::~RestoredTab() = default;

bool SessionRestoreDelegate::RestoredTab::operator<(
    const RestoredTab& right) const {
  // Tab with internal web UI like NTP or Settings are good choices to
  // defer loading.
  if (is_internal_page_ != right.is_internal_page_)
    return !is_internal_page_;
  // Pinned tabs should be loaded first.
  if (is_pinned_ != right.is_pinned_)
    return is_pinned_;
  // Apps should be loaded before normal tabs.
  if (is_app_ != right.is_app_)
    return is_app_;
  // Finally, older tabs should be deferred first.
  return contents_->GetLastActiveTimeTicks() >
         right.contents_->GetLastActiveTimeTicks();
}

// static
void SessionRestoreDelegate::RestoreTabs(
    const std::vector<RestoredTab>& tabs,
    const base::TimeTicks& restore_started) {
  if (tabs.empty())
    return;

  // Restore the favicon for all tabs. Any tab may end up being deferred due
  // to memory pressure so it's best to have some visual indication of its
  // contents.
  for (const auto& restored_tab : tabs) {
    CHECK(restored_tab.contents());
    // Restore the favicon for deferred tabs.
    favicon::ContentFaviconDriver* favicon_driver =
        favicon::ContentFaviconDriver::FromWebContents(restored_tab.contents());
    if (favicon_driver) {
      favicon_driver->FetchFavicon(favicon_driver->GetActiveURL(),
                                   /*is_same_document=*/false);
    }
  }

  SessionRestoreStatsCollector::GetOrCreateInstance(
      restore_started,
      std::make_unique<
          SessionRestoreStatsCollector::UmaStatsReportingDelegate>())
      ->TrackTabs(tabs);

  // Don't start a TabLoader here if background tab loading is done by
  // PerformanceManager.
  if (!base::FeatureList::IsEnabled(
          performance_manager::features::
              kBackgroundTabLoadingFromPerformanceManager)) {
    TabLoader::RestoreTabs(tabs, restore_started);
  } else {
    std::vector<content::WebContents*> web_contents_vector;
    web_contents_vector.reserve(tabs.size());
    for (auto tab : tabs) {
      CHECK(tab.contents());
      web_contents_vector.push_back(tab.contents());
    }
    performance_manager::policies::ScheduleLoadForRestoredTabs(
        std::move(web_contents_vector));
  }
}
