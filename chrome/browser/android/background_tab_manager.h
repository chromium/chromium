// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BACKGROUND_TAB_MANAGER_H_
#define CHROME_BROWSER_ANDROID_BACKGROUND_TAB_MANAGER_H_

#include <memory>
#include <vector>

#include "base/memory/singleton.h"
#include "content/public/browser/web_contents_observer.h"

class Profile;

namespace content {
class WebContents;
}

namespace history {
struct HistoryAddPageArgs;
class HistoryService;
}

namespace chrome {
namespace android {

class BackgroundTabManager;

class WebContentsDestroyedObserver : public content::WebContentsObserver {
 public:
  WebContentsDestroyedObserver(BackgroundTabManager* owner,
                               content::WebContents* watched_contents);
  ~WebContentsDestroyedObserver() override;

  // WebContentsObserver:
  void WebContentsDestroyed() override;

 private:
  BackgroundTabManager* owner_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsDestroyedObserver);
};

// BackgroundTabManager is responsible for storing state for the current
// background tab if any. To uniquely identify a tab we use a pointer to its
// WebContents. State managed include :
// - The Profile that is associated with the content.
// - Browser History which is cached when the tab is hidden and committed when
//   shown.
// This class is a global singleton.
// All methods should be called on the UI thread.
class BackgroundTabManager {
 public:
  BackgroundTabManager();

  ~BackgroundTabManager();

  // Return wether this WebContents is currently identified as being part of a
  // background tab.
  bool IsBackgroundTab(content::WebContents* web_contents) const;

  // Register the WebContents as the content for the current background tab.
  // At most one tab can be registered as a background tab.
  void RegisterBackgroundTab(content::WebContents* web_contents,
                             Profile* profile);

  // Manually unregister a WebContents. Called automatically when the registered
  // WebContents is destroyed. Clear all state.
  void UnregisterBackgroundTab();

  // Retrieves the profile that was stored during background tab registration.
  Profile* GetProfile() const;

  // Cache a single history item, to be either used by CommitHistory() or
  // discarded by UnregisterBackgroundTab().
  void CacheHistory(const history::HistoryAddPageArgs& history_item);

  // Commit the history that was previously cached for this tab. Committing
  // history clears it from the local cache.
  void CommitHistory(history::HistoryService* history_service);

  static BackgroundTabManager* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<BackgroundTabManager>;

  content::WebContents* web_contents_;
  Profile* profile_;
  std::vector<history::HistoryAddPageArgs> cached_history_;
  std::unique_ptr<WebContentsDestroyedObserver> web_contents_observer_;
};

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_BACKGROUND_TAB_MANAGER_H_
