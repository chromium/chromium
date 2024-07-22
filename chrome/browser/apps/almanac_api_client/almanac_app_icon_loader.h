// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_ALMANAC_API_CLIENT_ALMANAC_APP_ICON_LOADER_H_
#define CHROME_BROWSER_APPS_ALMANAC_API_CLIENT_ALMANAC_APP_ICON_LOADER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/services/app_service/public/cpp/icon_types.h"

class GURL;
class Profile;

namespace gfx {
class Image;
}  // namespace gfx

namespace apps {

class AlmanacIconCache;

// Generic app icon loader for the different use cases of the Almanac server.
// Similar to AlmanacIconCache except it also supports SVG icons and applies app
// masking consistent with App Service.
class AlmanacAppIconLoader {
 public:
  explicit AlmanacAppIconLoader(Profile& profile);

  virtual ~AlmanacAppIconLoader();

  AlmanacAppIconLoader(const AlmanacAppIconLoader&) = delete;
  AlmanacAppIconLoader& operator=(const AlmanacAppIconLoader&) = delete;

  // Downloads `icon_url` from the internet, decodes/rasterizes it and applies
  // ChromeOS app icon mask filtering to match what is seen in the
  // launcher/shelf.
  // Note that if the mimetype is SVG then a background WebContents will be spun
  // up and used to render the icon, this WebContents lives until the
  // AlmanacAppIconLoader is destroyed. Be sure to clean up this loader when not
  // in use.
  void GetAppIcon(const GURL& icon_url,
                  std::string_view icon_mime_type,
                  bool icon_masking_allowed,
                  base::OnceCallback<void(apps::IconValuePtr)> callback);

 private:
  class SvgLoader;

  void OnSvgLoaded(bool icon_masking_allowed,
                   base::OnceCallback<void(apps::IconValuePtr)> callback,
                   std::optional<SkBitmap> bitmap);

  void OnBitmapLoaded(bool icon_masking_allowed,
                      base::OnceCallback<void(apps::IconValuePtr)> callback,
                      const gfx::Image& icon_bitmap);

  void ApplyIconEffects(bool icon_masking_allowed,
                        gfx::Image icon_bitmap,
                        base::OnceCallback<void(apps::IconValuePtr)> callback);

  base::WeakPtr<Profile> profile_;
  std::unique_ptr<SvgLoader> svg_loader_;
  std::unique_ptr<AlmanacIconCache> icon_cache_;

  base::WeakPtrFactory<AlmanacAppIconLoader> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_ALMANAC_API_CLIENT_ALMANAC_APP_ICON_LOADER_H_
