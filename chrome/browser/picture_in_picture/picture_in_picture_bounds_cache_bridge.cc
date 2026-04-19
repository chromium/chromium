// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <optional>

#include "base/android/jni_array.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_bounds_cache.h"
#include "content/public/browser/web_contents.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/PictureInPictureBoundsCacheBridge_jni.h"

using base::android::ScopedJavaLocalRef;

namespace picture_in_picture {

static ScopedJavaLocalRef<jintArray>
JNI_PictureInPictureBoundsCacheBridge_GetBoundsForNewWindow(
    JNIEnv* env,
    content::WebContents* web_contents,
    int32_t opener_display_id,
    int32_t requested_width,
    int32_t requested_height) {
  if (!web_contents) {
    return nullptr;
  }

  display::Display opener_display;
  opener_display.set_id(opener_display_id);

  std::optional<gfx::Size> requested_size;
  // A width/height of -1 (or <= 0) indicates that no size was requested.
  if (requested_width > 0 && requested_height > 0) {
    requested_size.emplace(requested_width, requested_height);
  }

  std::optional<gfx::Rect> bounds =
      PictureInPictureBoundsCache::GetBoundsForNewWindow(
          web_contents, opener_display, requested_size);

  if (!bounds) {
    return nullptr;
  }

  const auto coords = std::to_array<const int>(
      {bounds->x(), bounds->y(), bounds->right(), bounds->bottom()});
  return base::android::ToJavaIntArray(env, coords);
}

static void JNI_PictureInPictureBoundsCacheBridge_UpdateCachedBounds(
    JNIEnv* env,
    content::WebContents* web_contents,
    int32_t left,
    int32_t top,
    int32_t right,
    int32_t bottom,
    int32_t opener_display_id,
    int32_t pip_display_id) {
  if (!web_contents) {
    return;
  }

  display::Display opener_display;
  opener_display.set_id(opener_display_id);

  display::Display pip_display;
  pip_display.set_id(pip_display_id);

  gfx::Rect bounds(left, top, right - left, bottom - top);

  PictureInPictureBoundsCache::UpdateCachedBounds(web_contents, bounds,
                                                  opener_display, pip_display);
}

static void JNI_PictureInPictureBoundsCacheBridge_ClearCachedBounds(
    JNIEnv* env,
    content::WebContents* web_contents) {
  if (!web_contents) {
    return;
  }

  PictureInPictureBoundsCache::ClearCachedBounds(web_contents);
}

}  // namespace picture_in_picture

DEFINE_JNI(PictureInPictureBoundsCacheBridge)
