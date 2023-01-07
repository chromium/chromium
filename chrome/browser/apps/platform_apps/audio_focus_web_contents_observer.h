// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_PLATFORM_APPS_AUDIO_FOCUS_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_APPS_PLATFORM_APPS_AUDIO_FOCUS_WEB_CONTENTS_OBSERVER_H_

#include "base/unguessable_token.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace apps {

// AudioFocusWebContentsObserver manages audio focus group ids for apps. This
// means that apps will have seperate audio focus from the browser.
class AudioFocusWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<AudioFocusWebContentsObserver> {
 public:
  AudioFocusWebContentsObserver(const AudioFocusWebContentsObserver&) = delete;
  AudioFocusWebContentsObserver& operator=(
      const AudioFocusWebContentsObserver&) = delete;
  ~AudioFocusWebContentsObserver() override;

 private:
  friend class content::WebContentsUserData<AudioFocusWebContentsObserver>;
  friend class AudioFocusWebContentsObserverBrowserTest;

  explicit AudioFocusWebContentsObserver(content::WebContents*);

  // content::WebContentsObserver overrides.
  void PrimaryPageChanged(content::Page&) override;

  // The audio focus group id is used to group media sessions together for apps.
  base::UnguessableToken audio_focus_group_id_ = base::UnguessableToken::Null();

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_PLATFORM_APPS_AUDIO_FOCUS_WEB_CONTENTS_OBSERVER_H_
