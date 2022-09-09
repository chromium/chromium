// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_GALLERY_WATCH_MANAGER_OBSERVER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_GALLERY_WATCH_MANAGER_OBSERVER_H_

#include "chrome/browser/media_galleries/media_galleries_preferences.h"

class GalleryWatchManagerObserver {
 public:
  virtual ~GalleryWatchManagerObserver() {}

  // Called when the gallery contents change.
  virtual void OnGalleryChanged(const std::string& extension_id,
                                MediaGalleryPrefId gallery_id) = 0;

  // Called when the gallery watch is dropped without the caller requesting it,
  // because the permission was revoked, device was detached, etc.
  virtual void OnGalleryWatchDropped(const std::string& extension_id,
                                     MediaGalleryPrefId gallery_id) = 0;
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_GALLERY_WATCH_MANAGER_OBSERVER_H_
