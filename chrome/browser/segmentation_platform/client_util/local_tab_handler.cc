// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/client_util/local_tab_handler.h"

#include "base/time/time.h"
#include "components/segmentation_platform/embedder/input_delegate/tab_session_source.h"
#include "components/segmentation_platform/embedder/tab_fetcher.h"
#include "components/segmentation_platform/internal/execution/processing/feature_processor_state.h"
#include "components/segmentation_platform/public/input_delegate.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_ANDROID)

#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"

#else  // BUILDFLAG(IS_ANDROID)

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/sync/browser_synced_tab_delegate.h"

#endif  // BUILDFLAG(IS_ANDROID)

namespace segmentation_platform::processing {

namespace {

GURL GetLocalTabURL(const TabFetcher::Tab& tab) {
  if (tab.webcontents) {
    return tab.webcontents->GetURL();
  }
#if BUILDFLAG(IS_ANDROID)
  if (tab.tab_android) {
    return tab.tab_android->GetURL();
  }
#endif
  return GURL();
}

// Returns the time since last time the tab was modified.
base::TimeDelta GetLocalTimeSinceModified(const TabFetcher::Tab& tab) {
  base::Time last_modified_timestamp;
  if (tab.webcontents) {
    auto* last_entry = tab.webcontents->GetController().GetLastCommittedEntry();
    if (last_entry) {
      last_modified_timestamp = last_entry->GetTimestamp();
    }
  }
#if BUILDFLAG(IS_ANDROID)
  if (tab.tab_android) {
    last_modified_timestamp = tab.tab_android->GetLastShownTimestamp();
  }
#endif
  return base::Time::Now() - last_modified_timestamp;
}

#if BUILDFLAG(IS_ANDROID)

// Returns a list of all tabs from tab model.
std::vector<TabFetcher::TabEntry> FetchTabs(const Profile* profile) {
  std::vector<TabFetcher::TabEntry> tabs;
  for (const TabModel* model : TabModelList::models()) {
    if (model->GetProfile() != profile) {
      continue;
    }
    // Store count in local variable since it makes expensive JNI call.
    int count = model->GetTabCount();
    for (int i = 0; i < count; ++i) {
      auto* web_contents = model->GetWebContentsAt(i);
      auto* tab_android = model->GetTabAt(i);
      auto tab_id = tab_android->GetSyncedTabDelegate()->GetSessionId();
      tabs.emplace_back(tab_id, web_contents, tab_android);
    }
  }
  return tabs;
}

TabFetcher::Tab FindLocalTabAndroid(const Profile* profile,
                                    const TabFetcher::TabEntry& entry) {
  for (const TabModel* model : TabModelList::models()) {
    if (model->GetProfile() != profile) {
      continue;
    }
    // Store count in local variable since it makes expensive JNI call.
    int count = model->GetTabCount();
    for (int i = 0; i < count; ++i) {
      auto* tab_android = model->GetTabAt(i);
      SessionID id = tab_android->GetSyncedTabDelegate()->GetSessionId();
      if (id != entry.tab_id) {
        continue;
      }
      TabFetcher::Tab result;
      result.webcontents = model->GetWebContentsAt(i);
      result.tab_android = tab_android;
      result.time_since_modified = GetLocalTimeSinceModified(result);
      result.tab_url = GetLocalTabURL(result);
      return result;
    }
  }
  return TabFetcher::Tab();
}

#else  // BUILDFLAG(IS_ANDROID)

// Returns a list of all tabs from tab strip model.
std::vector<TabFetcher::TabEntry> FetchTabs(const Profile* profile) {
  const BrowserList* browser_list = BrowserList::GetInstance();
  std::vector<TabFetcher::TabEntry> tabs;
  for (const Browser* browser : *browser_list) {
    if (browser->profile() != profile) {
      continue;
    }
    for (int i = 0; i < browser->tab_strip_model()->GetTabCount(); ++i) {
      auto* web_contents = browser->tab_strip_model()->GetWebContentsAt(i);
      auto* tab_delegate =
          BrowserSyncedTabDelegate::FromWebContents(web_contents);
      tabs.emplace_back(tab_delegate->GetSessionId(), web_contents, nullptr);
    }
  }
  return tabs;
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

LocalTabHandler::LocalTabHandler(
    sync_sessions::SessionSyncService* session_sync_service,
    Profile* profile)
    : TabFetcher(session_sync_service), profile_(profile) {}

LocalTabHandler::~LocalTabHandler() = default;

bool LocalTabHandler::FillAllLocalTabsFromTabModel(
    std::vector<TabEntry>& tabs) {
  tabs = FetchTabs(profile_);
  return true;
}

TabFetcher::Tab LocalTabHandler::FindLocalTab(const TabEntry& entry) {
#if BUILDFLAG(IS_ANDROID)
  return FindLocalTabAndroid(profile_, entry);
#else
  TabFetcher::Tab result;
  // Fetch all tabs and verify if the `entry` is still valid.
  auto all_local_tabs = FetchTabs(profile_);
  for (auto& tab : all_local_tabs) {
    if (tab.tab_id == entry.tab_id) {
      result.webcontents =
          reinterpret_cast<content::WebContents*>(tab.web_contents_data.get());
      result.tab_android =
          reinterpret_cast<TabAndroid*>(tab.tab_android_data.get());
      result.time_since_modified = GetLocalTimeSinceModified(result);
      result.tab_url = GetLocalTabURL(result);
      break;
    }
  }
  return result;
#endif
}

LocalTabSource::LocalTabSource(
    sync_sessions::SessionSyncService* session_sync_service,
    TabFetcher* tab_fetcher)
    : TabSessionSource(session_sync_service, tab_fetcher) {}

LocalTabSource::~LocalTabSource() = default;

void LocalTabSource::AddLocalTabInfo(
    const TabFetcher::Tab& tab,
    FeatureProcessorState& feature_processor_state,
    Tensor& inputs) {
  inputs[TabSessionSource::kInputLocalTabTimeSinceModified] =
      ProcessedValue::FromFloat(
          BucketizeExp(GetLocalTimeSinceModified(tab).InSeconds(), /*max_buckets*/50));
}

}  // namespace segmentation_platform::processing
