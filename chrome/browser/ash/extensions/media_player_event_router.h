// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_MEDIA_PLAYER_EVENT_ROUTER_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_MEDIA_PLAYER_EVENT_ROUTER_H_

#include "base/memory/raw_ptr.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// Event router class for events related to Mediaplayer.
class MediaPlayerEventRouter {
 public:
  explicit MediaPlayerEventRouter(content::BrowserContext* context);

  MediaPlayerEventRouter(const MediaPlayerEventRouter&) = delete;
  MediaPlayerEventRouter& operator=(const MediaPlayerEventRouter&) = delete;

  virtual ~MediaPlayerEventRouter();

  // Send notification that next-track shortcut key was pressed.
  void NotifyNextTrack();

  // Send notification that previous-track shortcut key was pressed.
  void NotifyPrevTrack();

  // Send notification that play/pause shortcut key was pressed.
  void NotifyTogglePlayState();

 private:
  raw_ptr<content::BrowserContext> browser_context_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_MEDIA_PLAYER_EVENT_ROUTER_H_
