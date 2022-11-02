// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/background_tab_manager.h"

#include "base/memory/singleton.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace chrome {
namespace android {

static std::map<content::WebContents *, chrome::android::BackgroundTabItem*> background_map_ = {};

WebContentsDestroyedObserver::WebContentsDestroyedObserver(
    content::WebContents* watched_contents)
    : content::WebContentsObserver(watched_contents) {}

WebContentsDestroyedObserver::~WebContentsDestroyedObserver() {}

void WebContentsDestroyedObserver::WebContentsDestroyed() {
    BackgroundTabManager::UnregisterBackgroundTab(web_contents());
}

bool BackgroundTabManager::IsBackgroundTab(content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return !web_contents && background_map_.find(web_contents) != background_map_.end();
}

void BackgroundTabManager::RegisterBackgroundTab(
    content::WebContents* web_contents,
    Profile* profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  chrome::android::BackgroundTabItem* item = new BackgroundTabItem();
  item->profile_ = profile;
  item->web_contents_observer_ =
          std::make_unique<WebContentsDestroyedObserver>(web_contents);
  background_map_.insert(std::pair(web_contents, item));
}

void BackgroundTabManager::UnregisterBackgroundTab(content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BackgroundTabItem* item = findBackgroundTabItem(web_contents);
  if (item) {
      item->profile_ = nullptr;
      item->cached_history_.clear();
      item->web_contents_observer_.reset();
      background_map_.erase(web_contents);
  }
}

Profile* BackgroundTabManager::GetProfile(content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BackgroundTabItem* item = findBackgroundTabItem(web_contents);
  if (item) {
      return item->profile_;
  }
  return nullptr;

}

void BackgroundTabManager::CacheHistory(
    content::WebContents* web_contents,
    const history::HistoryAddPageArgs& history_item) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BackgroundTabItem* item = findBackgroundTabItem(web_contents);
  if (item) {
      item->cached_history_.push_back(history_item);
  }
}

void BackgroundTabManager::CommitHistory(
    content::WebContents* web_contents,
    history::HistoryService* history_service) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!history_service) {
      return;
  }
  // History service can be null in non exceptional conditions, e.g. incognito
  // mode. We clear the cached history in any case.
  BackgroundTabItem* item = findBackgroundTabItem(web_contents);
  if (item) {
      for (const auto& history_item : item->cached_history_) {
          history_service->AddPage(history_item);
      }
      item->cached_history_.clear();
  }
}

BackgroundTabItem* BackgroundTabManager::findBackgroundTabItem(content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto it = background_map_.find(web_contents);
  if (it != background_map_.end()) {
      return it->second;
  }
  return nullptr;
}

}  // namespace android
}  // namespace chrome
