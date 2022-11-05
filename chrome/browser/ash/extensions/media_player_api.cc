// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/media_player_api.h"

#include <memory>

#include "base/lazy_instance.h"
#include "base/values.h"
#include "chrome/browser/ash/extensions/media_player_event_router.h"

namespace extensions {

MediaPlayerAPI::MediaPlayerAPI(content::BrowserContext* context)
    : browser_context_(context) {}

MediaPlayerAPI::~MediaPlayerAPI() = default;

// static
MediaPlayerAPI* MediaPlayerAPI::Get(content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<MediaPlayerAPI>::Get(context);
}

MediaPlayerEventRouter* MediaPlayerAPI::media_player_event_router() {
  if (!media_player_event_router_)
    media_player_event_router_ =
        std::make_unique<MediaPlayerEventRouter>(browser_context_);
  return media_player_event_router_.get();
}

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<MediaPlayerAPI>>::DestructorAtExit g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<MediaPlayerAPI>*
MediaPlayerAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

}  // namespace extensions
