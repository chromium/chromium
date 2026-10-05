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

BackgroundTabManager::BackgroundTabManager(content::WebContents* web_contents,
                                           Profile* profile)
    : content::WebContentsUserData<BackgroundTabManager>(*web_contents) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  profile_ = profile;
}

BackgroundTabManager::~BackgroundTabManager() = default;

void BackgroundTabManager::UnregisterBackgroundTab() {
  GetWebContents().RemoveUserData(UserDataKey());
  // NOTE: |this| is deleted at this point, do not add logic here.
}

Profile* BackgroundTabManager::GetProfile() const {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  return profile_;
}

void BackgroundTabManager::CacheHistory(
    const history::HistoryAddPageArgs& history_item) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  cached_history_.push_back(history_item);
}

void BackgroundTabManager::CommitHistory(
    history::HistoryService* history_service) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  // History service can be null in non exceptional conditions, e.g. incognito
  // mode. We clear the cached history in any case.
  if (history_service) {
    for (const auto& history_item : cached_history_) {
      history_service->AddPage(history_item);
    }
  }
  cached_history_.clear();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(BackgroundTabManager);

}  // namespace android
}  // namespace chrome
