// Copyright 2017 The Chromium Authors
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

WebContentsDestroyedObserver::WebContentsDestroyedObserver(
    BackgroundTabManager* owner,
    content::WebContents* watched_contents)
    : content::WebContentsObserver(watched_contents), owner_(owner) {}

WebContentsDestroyedObserver::~WebContentsDestroyedObserver() {}

void WebContentsDestroyedObserver::WebContentsDestroyed() {
  DCHECK(owner_->IsBackgroundTab(web_contents()));
  owner_->UnregisterBackgroundTab();
}

BackgroundTabManager::BackgroundTabManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  web_contents_ = nullptr;
  profile_ = nullptr;
}

BackgroundTabManager::~BackgroundTabManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  web_contents_ = nullptr;
  profile_ = nullptr;
}

bool BackgroundTabManager::IsBackgroundTab(
    content::WebContents* web_contents) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!web_contents)
    return false;
  return web_contents_ == web_contents;
}

void BackgroundTabManager::RegisterBackgroundTab(
    content::WebContents* web_contents,
    Profile* profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!web_contents_);
  web_contents_ = web_contents;
  profile_ = profile;
  web_contents_observer_ =
      std::make_unique<WebContentsDestroyedObserver>(this, web_contents);
}

void BackgroundTabManager::UnregisterBackgroundTab() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(web_contents_);
  web_contents_ = nullptr;
  profile_ = nullptr;
  cached_history_.clear();
  web_contents_observer_.reset();
}

Profile* BackgroundTabManager::GetProfile() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return profile_;
}

void BackgroundTabManager::CacheHistory(
    const history::HistoryAddPageArgs& history_item) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  cached_history_.push_back(history_item);
}

void BackgroundTabManager::CommitHistory(
    history::HistoryService* history_service) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // History service can be null in non exceptional conditions, e.g. incognito
  // mode. We clear the cached history in any case.
  if (history_service) {
    for (const auto& history_item : cached_history_) {
      history_service->AddPage(history_item);
    }
  }
  cached_history_.clear();
}

BackgroundTabManager* BackgroundTabManager::GetInstance() {
  return base::Singleton<BackgroundTabManager>::get();
}

}  // namespace android
}  // namespace chrome
