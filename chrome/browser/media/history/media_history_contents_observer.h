// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_CONTENTS_OBSERVER_H_

#include "chrome/browser/media/history/media_history_keyed_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class MediaHistoryContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<MediaHistoryContentsObserver> {
 public:
  ~MediaHistoryContentsObserver() override;

  void MediaWatchTimeChanged(
      const content::MediaPlayerWatchTime& watch_time) override;

 private:
  friend class content::WebContentsUserData<MediaHistoryContentsObserver>;

  explicit MediaHistoryContentsObserver(content::WebContents* web_contents);

  media_history::MediaHistoryKeyedService* service_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(MediaHistoryContentsObserver);
};

#endif  // CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_CONTENTS_OBSERVER_H_
