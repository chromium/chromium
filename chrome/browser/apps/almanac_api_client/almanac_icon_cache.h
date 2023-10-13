// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_ALMANAC_API_CLIENT_ALMANAC_ICON_CACHE_H_
#define CHROME_BROWSER_APPS_ALMANAC_API_CLIENT_ALMANAC_ICON_CACHE_H_

#include "base/functional/callback.h"

class GURL;
class ProfileKey;

namespace image_fetcher {
class ImageFetcher;
}  // namespace image_fetcher

namespace gfx {
class Image;
}  // namespace gfx

namespace apps {

// Generic icon cache for the different use cases of the Almanac server.
class AlmanacIconCache {
 public:
  explicit AlmanacIconCache(ProfileKey* key);
  virtual ~AlmanacIconCache();

  // Downloads the icon for the specified GURL.
  virtual void GetIcon(const GURL& icon_url,
                       base::OnceCallback<void(const gfx::Image&)> callback);

 protected:
  AlmanacIconCache();

  // Method is overridden for the mock version of the image fetcher to
  // propagate.
  virtual image_fetcher::ImageFetcher* GetImageFetcher();

 private:
  raw_ptr<image_fetcher::ImageFetcher> image_fetcher_ = nullptr;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_ALMANAC_API_CLIENT_ALMANAC_ICON_CACHE_H_
