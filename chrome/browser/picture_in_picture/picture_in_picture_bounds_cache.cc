// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/picture_in_picture_bounds_cache.h"

#include "content/public/browser/web_contents.h"
#include "ui/display/display.h"

WEB_CONTENTS_USER_DATA_KEY_IMPL(PictureInPictureBoundsCache);

PictureInPictureBoundsCache::PictureInPictureBoundsCache(
    content::WebContents* web_contents)
    : content::WebContentsUserData<PictureInPictureBoundsCache>(*web_contents) {
}

PictureInPictureBoundsCache::~PictureInPictureBoundsCache() = default;

// static
std::optional<gfx::Rect> PictureInPictureBoundsCache::GetBoundsForNewWindow(
    content::WebContents* web_contents,
    const display::Display& display,
    std::optional<gfx::Size> requested_content_size) {
  CHECK(web_contents);

  // This is a no-op if the cache already exists.
  CreateForWebContents(web_contents);
  auto* cache = FromWebContents(web_contents);

  return cache->GetBoundsForNewWindow(display, requested_content_size);
}

// static
void PictureInPictureBoundsCache::UpdateCachedBounds(
    content::WebContents* web_contents,
    const gfx::Rect& most_recent_bounds) {
  auto* cache = FromWebContents(web_contents);
  CHECK(cache);
  return cache->UpdateCachedBounds(most_recent_bounds);
}

std::optional<gfx::Rect> PictureInPictureBoundsCache::GetBoundsForNewWindow(
    const display::Display& display,
    const std::optional<gfx::Size>& requested_content_size) {
  // The cache is only valid if the requested size match the most recently
  // cached size or there is no requested size, and the origins match.  In other
  // words, if the current request does not specify a size, then it will match
  // any previously requested size.  If the cached entry was for a pip window
  // that did not request an initial size, then it can only match a new request
  // that also does not request one.
  auto current_origin =
      GetWebContents().GetPrimaryMainFrame()->GetLastCommittedOrigin();
  if (current_origin != origin_ || display.id() != display_id_ ||
      (requested_content_size &&
       requested_content_size != requested_content_size_)) {
    // The existing cached entry is not applicable, so forget it and start
    // caching this new request.
    Clear();
    origin_ = current_origin;
    display_id_ = display.id();
    requested_content_size_ = requested_content_size;
    return std::nullopt;
  }

  // If we have most recent bounds, then send them.  It's possible that we
  // don't, for example if this is a new cache entry that was created for the
  // current request.
  return most_recent_bounds_;
}

void PictureInPictureBoundsCache::UpdateCachedBounds(
    const gfx::Rect& most_recent_bounds) {
  most_recent_bounds_ = most_recent_bounds;
}

void PictureInPictureBoundsCache::Clear() {
  origin_ = url::Origin();
  display_id_ = -1;  // "no display", according to //ui/display/display.h
  requested_content_size_.reset();
  most_recent_bounds_.reset();
}
