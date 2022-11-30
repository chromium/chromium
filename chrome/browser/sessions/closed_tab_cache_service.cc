// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/closed_tab_cache_service.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/closed_tab_cache.h"

ClosedTabCacheService::ClosedTabCacheService(Profile* profile)
    : profile_(profile), cache_(std::make_unique<ClosedTabCache>()) {
  // Incognito profiles don't allow tab restores.
  DCHECK(!profile_->IsOffTheRecord());
}

ClosedTabCacheService::~ClosedTabCacheService() = default;

void ClosedTabCacheService::Shutdown() {
  cache_.reset();
}

ClosedTabCache& ClosedTabCacheService::closed_tab_cache() {
  DCHECK(cache_);
  return *cache_.get();
}
