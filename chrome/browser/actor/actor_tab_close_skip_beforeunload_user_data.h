// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_TAB_CLOSE_SKIP_BEFOREUNLOAD_USER_DATA_H_
#define CHROME_BROWSER_ACTOR_ACTOR_TAB_CLOSE_SKIP_BEFOREUNLOAD_USER_DATA_H_

#include "content/public/browser/web_contents_user_data.h"

namespace actor {

// A lightweight tag attached to a WebContents to indicate that the user has
// explicitly confirmed they want to close the tab via the confirmation dialog.
//
// During single-tab closure, when this tag is present:
// 1. UnloadController will not query the confirmation dialog again for this
// tab.
// 2. For tabs without JS beforeunload listeners, UnloadController proceeds
//    directly with tab closure.
class ActorTabCloseSkipBeforeUnloadUserData
    : public content::WebContentsUserData<
          ActorTabCloseSkipBeforeUnloadUserData> {
 public:
  ~ActorTabCloseSkipBeforeUnloadUserData() override = default;

  static void DeleteForWebContents(content::WebContents* contents) {
    if (contents) {
      contents->RemoveUserData(UserDataKey());
    }
  }

 private:
  explicit ActorTabCloseSkipBeforeUnloadUserData(content::WebContents* contents)
      : content::WebContentsUserData<ActorTabCloseSkipBeforeUnloadUserData>(
            *contents) {}
  friend class content::WebContentsUserData<
      ActorTabCloseSkipBeforeUnloadUserData>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_TAB_CLOSE_SKIP_BEFOREUNLOAD_USER_DATA_H_
