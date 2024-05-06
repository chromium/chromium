// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BACKGROUND_TAB_MANAGER_H_
#define CHROME_BROWSER_ANDROID_BACKGROUND_TAB_MANAGER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "content/public/browser/web_contents_user_data.h"

class Profile;

namespace history {
struct HistoryAddPageArgs;
class HistoryService;
}

namespace chrome {
namespace android {

class BackgroundTabManager;

// BackgroundTabManager is responsible for storing state backgrounded tabs.
// State managed include :
// - The Profile that is associated with the content.
// - Browser History which is cached when the tab is hidden and committed when
//   shown.
// All methods should be called on the UI thread.
class BackgroundTabManager
    : public content::WebContentsUserData<BackgroundTabManager> {
 public:
  BackgroundTabManager(content::WebContents* web_contents, Profile* profile);

  ~BackgroundTabManager() override;

  // Called when the Tab is no longer a background tab. Unregisters the
  // UserData.
  void UnregisterBackgroundTab();

  // Retrieves the profile that was stored during background tab registration.
  Profile* GetProfile() const;

  // Cache a single history item, to be either used by CommitHistory() or
  // discarded by UnregisterBackgroundTab().
  void CacheHistory(const history::HistoryAddPageArgs& history_item);

  // Commit the history that was previously cached for this tab. Committing
  // history clears it from the local cache.
  void CommitHistory(history::HistoryService* history_service);

 private:
  friend class content::WebContentsUserData<BackgroundTabManager>;

  raw_ptr<Profile> profile_;
  std::vector<history::HistoryAddPageArgs> cached_history_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_BACKGROUND_TAB_MANAGER_H_
