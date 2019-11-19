// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_sinks_observer.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/browser/media/router/media_router.h"

#if DCHECK_IS_ON()
#include "base/auto_reset.h"
#endif

namespace media_router {

MediaSinksObserver::MediaSinksObserver(MediaRouter* router,
                                       const MediaSource& source,
                                       const url::Origin& origin)
    : source_(source), origin_(origin), router_(router), initialized_(false) {
  DCHECK(router_);
}

MediaSinksObserver::MediaSinksObserver(MediaRouter* router)
    : router_(router), initialized_(false) {
  DCHECK(router_);
}

MediaSinksObserver::~MediaSinksObserver() {
#if DCHECK_IS_ON()
  DCHECK(!in_on_sinks_updated_);
#endif

  if (initialized_)
    router_->UnregisterMediaSinksObserver(this);
}

bool MediaSinksObserver::Init() {
  if (initialized_)
    return true;

  initialized_ = router_->RegisterMediaSinksObserver(this);
  return initialized_;
}

void MediaSinksObserver::OnSinksUpdated(
    const std::vector<MediaSink>& sinks,
    const std::vector<url::Origin>& origins) {
#if DCHECK_IS_ON()
  base::AutoReset<bool> reset_in_on_sinks_updated(&in_on_sinks_updated_, true);
#endif

  if (origins.empty() || base::Contains(origins, origin_))
    OnSinksReceived(sinks);
  else
    OnSinksReceived(std::vector<MediaSink>());
}

}  // namespace media_router
