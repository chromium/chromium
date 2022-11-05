// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/media_player_event_router.h"

#include <utility>

#include "base/memory/singleton.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"

namespace extensions {

namespace {

void BroadcastEvent(content::BrowserContext* context,
                    events::HistogramValue histogram_value,
                    const std::string& event_name) {
  if (context && EventRouter::Get(context)) {
    std::unique_ptr<Event> event(
        new Event(histogram_value, event_name, base::Value::List()));
    EventRouter::Get(context)->BroadcastEvent(std::move(event));
  }
}

}  // namespace

MediaPlayerEventRouter::MediaPlayerEventRouter(content::BrowserContext* context)
    : browser_context_(context) {}

MediaPlayerEventRouter::~MediaPlayerEventRouter() = default;

void MediaPlayerEventRouter::NotifyNextTrack() {
  BroadcastEvent(browser_context_, events::MEDIA_PLAYER_PRIVATE_ON_NEXT_TRACK,
                 "mediaPlayerPrivate.onNextTrack");
}

void MediaPlayerEventRouter::NotifyPrevTrack() {
  BroadcastEvent(browser_context_, events::MEDIA_PLAYER_PRIVATE_ON_PREV_TRACK,
                 "mediaPlayerPrivate.onPrevTrack");
}

void MediaPlayerEventRouter::NotifyTogglePlayState() {
  BroadcastEvent(browser_context_,
                 events::MEDIA_PLAYER_PRIVATE_ON_TOGGLE_PLAY_STATE,
                 "mediaPlayerPrivate.onTogglePlayState");
}

}  // namespace extensions
