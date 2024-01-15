// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_BOUNDS_CACHE_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_BOUNDS_CACHE_H_

#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

namespace display {
class Display;
}  // namespace display

// Per-WebContents cache of the most recent bounds for a picture in picture
// window.  This is forgotten when the WebContents navigates to a new origin.
// If the site requests the same content (not window) size, and if it's the same
// origin, then we will try to provide the most recent window bounds from the
// previous pip window.  This allows the user to move / resize the pip window,
// and have it stay there even if the site closes and re-opens it.
//
// None of this is stored permanently.
class PictureInPictureBoundsCache
    : public content::WebContentsUserData<PictureInPictureBoundsCache> {
 public:
  ~PictureInPictureBoundsCache() override;

  // Return the cached window bounds to use for a given site-requested content
  // size for `web_contents`.  The returned bounds may be null if there is no
  // match, in which case the default bounds should be used.  If window bounds
  // are returned, then they might not necessarily reflect in a window that
  // hosts content of the requested content size.
  //
  // If the site did not request a particular content size, then
  // `requested_content_size` should be unset.
  //
  // `display` should reflect the display on which the opener is shown.
  //
  // This will also set up the cache to allow additional calls to
  // `UpdateCachedBounds()` to succeed.  This must be called first.
  static std::optional<gfx::Rect> GetBoundsForNewWindow(
      content::WebContents* web_contents,
      const display::Display& display,
      std::optional<gfx::Size> requested_content_size);

  // Updates the cache for `web_contents` to reflect `most_recent_bounds` as the
  // window (not content) bounds.  `GetBoundsForNewWindow()` must be called
  // before the first update.
  static void UpdateCachedBounds(content::WebContents* web_contents,
                                 const gfx::Rect& most_recent_bounds);

 private:
  friend class content::WebContentsUserData<PictureInPictureBoundsCache>;

  explicit PictureInPictureBoundsCache(content::WebContents* web_contents);

  // Given a new request for a pip window of the given requested size, on the
  // given display, initial the cache and return cached window bounds if they
  // match what's in the cache.
  std::optional<gfx::Rect> GetBoundsForNewWindow(
      const display::Display& display,
      const std::optional<gfx::Size>& requested_content_size);

  // Update the cache to reflect the most recent size of the window.
  void UpdateCachedBounds(const gfx::Rect& most_recent_bounds);

  // Reset our state to have no cached bounds.  Future calls to
  // `UpdaetCachedBounds()` will do nothing, until the next call to
  // `GetBoundsForNewWindow()` re-initializes the cache.
  void Clear();

  // The origin for this cache entry.
  url::Origin origin_;

  // Display from which the window was opened.  `-1` indicates "no display",
  // according to display::Display::id() docs.
  int64_t display_id_ = -1;

  // This is the most recent site-requested contents size, if any.
  std::optional<gfx::Size> requested_content_size_;

  // This is the most recent bounds for the pip window, which might differ if
  // it's resized / moved by the user.  Can be unset if we haven't been given
  // any bounds for the pip window yet.
  std::optional<gfx::Rect> most_recent_bounds_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_BOUNDS_CACHE_H_
