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
    const display::Display& opener_display,
    std::optional<gfx::Size> requested_content_size) {
  CHECK(web_contents);

  // This is a no-op if the cache already exists.
  CreateForWebContents(web_contents);
  auto* cache = FromWebContents(web_contents);

  return cache->GetBoundsForNewWindow(opener_display, requested_content_size);
}

// static
void PictureInPictureBoundsCache::UpdateCachedBounds(
    content::WebContents* web_contents,
    const gfx::Rect& most_recent_bounds,
    const display::Display& opener_display,
    const display::Display& pip_display) {
  auto* cache = FromWebContents(web_contents);
  CHECK(cache);
  return cache->UpdateCachedBounds(most_recent_bounds, opener_display,
                                   pip_display);
}

// static
void PictureInPictureBoundsCache::ClearCachedBounds(
    content::WebContents* web_contents) {
  auto* cache = FromWebContents(web_contents);
  CHECK(cache);
  cache->Clear();
}

std::optional<gfx::Rect> PictureInPictureBoundsCache::GetBoundsForNewWindow(
    const display::Display& opener_display,
    const std::optional<gfx::Size>& requested_content_size) {
  // The cache is only valid if the requested size match the most recently
  // cached size or there is no requested size, and the origins match.  In other
  // words, if the current request does not specify a size, then it will match
  // any previously requested size.  If the cached entry was for a pip window
  // that did not request an initial size, then it can only match a new request
  // that also does not request one.
  const bool requested_size_matches =
      !requested_content_size.has_value() ||
      (requested_content_size_ == requested_content_size);

  const bool opener_moved =
      opener_display_id_ && opener_display.id() != opener_display_id_.value();

  // The cache is invalid if the opener moved to a display that is different
  // from where the picture-in-picture window was. The window should follow the
  // opener. This is also the case if we don't know where the
  // picture-in-picture window was.
  const bool should_follow_opener =
      opener_moved &&
      (!pip_display_id_ || opener_display.id() != pip_display_id_.value());

  auto current_origin =
      GetWebContents().GetPrimaryMainFrame()->GetLastCommittedOrigin();
  if (current_origin != origin_ || !requested_size_matches ||
      should_follow_opener) {
    // The existing cached entry is not applicable, so forget it and start
    // caching this new request.
    Clear();
    origin_ = current_origin;
    opener_display_id_ = opener_display.id();
    requested_content_size_ = requested_content_size;
    return std::nullopt;
  }

  // The cache is valid, so update the opener's display id to reflect the
  // current display. This is needed to detect subsequent moves.
  opener_display_id_ = opener_display.id();

  return most_recent_bounds_;
}

void PictureInPictureBoundsCache::UpdateCachedBounds(
    const gfx::Rect& most_recent_bounds,
    const display::Display& opener_display,
    const display::Display& pip_display) {
  most_recent_bounds_ = most_recent_bounds;
  opener_display_id_ = opener_display.id();
  pip_display_id_ = pip_display.id();
}

void PictureInPictureBoundsCache::Clear() {
  origin_ = url::Origin();
  opener_display_id_.reset();
  pip_display_id_.reset();
  requested_content_size_.reset();
  most_recent_bounds_.reset();
}
