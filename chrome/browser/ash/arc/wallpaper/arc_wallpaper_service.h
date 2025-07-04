// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_WALLPAPER_ARC_WALLPAPER_SERVICE_H_
#define CHROME_BROWSER_ASH_ARC_WALLPAPER_ARC_WALLPAPER_SERVICE_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/experiences/arc/mojom/wallpaper.mojom.h"
#include "components/keyed_service/core/keyed_service.h"

class SkBitmap;

namespace content {
class BrowserContext;
}  // namespace content

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace arc {

class ArcBridgeService;

// Lives on the UI thread.
class ArcWallpaperService : public KeyedService, public mojom::WallpaperHost {
 public:
  class ImageDecoder {
   public:
    virtual ~ImageDecoder() = default;

    using ResultCallback = base::OnceCallback<void(const SkBitmap&)>;
    virtual void DecodeImage(const std::vector<uint8_t>& data,
                             ResultCallback callback) = 0;
  };

  static void EnsureFactoryBuilt();

  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcWallpaperService* GetForBrowserContext(
      content::BrowserContext* context);

  ArcWallpaperService(content::BrowserContext* context,
                      ArcBridgeService* bridge_service);

  ArcWallpaperService(const ArcWallpaperService&) = delete;
  ArcWallpaperService& operator=(const ArcWallpaperService&) = delete;

  ~ArcWallpaperService() override;

  // mojom::WallpaperHost overrides.
  void SetWallpaper(const std::vector<uint8_t>& data,
                    int32_t wallpaper_id) override;
  void SetDefaultWallpaper() override;
  void GetWallpaper(GetWallpaperCallback callback) override;

  // Replace a way to decode images for unittests.
  void SetImageDecoderForTesting(std::unique_ptr<ImageDecoder> decoder);

 private:
  friend class TestApi;
  class AndroidIdStore;

  void OnImageDecoded(int wallpaper_id, const SkBitmap& bitmap);

  // Initiates a set wallpaper request to //ash.
  void OnWallpaperDecoded(int32_t wallpaper_id, const gfx::ImageSkia& image);

  // Notifies wallpaper change if we have wallpaper instance.
  void NotifyWallpaperChanged(int wallpaper_id);

  // Notifies wallpaper change of |wallpaper_id|, then notify wallpaper change
  // of -1 to reset wallpaper cache at Android side.
  void NotifyWallpaperChangedAndReset(int wallpaper_id);

  const raw_ptr<ArcBridgeService>
      arc_bridge_service_;  // Owned by ArcServiceManager.

  std::unique_ptr<ImageDecoder> image_decoder_;

  // This is used to cancel decoding requests.
  base::WeakPtrFactory<ArcWallpaperService> weak_ptr_factory_for_decode_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_WALLPAPER_ARC_WALLPAPER_SERVICE_H_
