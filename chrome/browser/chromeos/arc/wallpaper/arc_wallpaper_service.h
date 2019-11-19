// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_WALLPAPER_ARC_WALLPAPER_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_WALLPAPER_ARC_WALLPAPER_SERVICE_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/image_decoder.h"
#include "components/arc/mojom/wallpaper.mojom.h"
#include "components/keyed_service/core/keyed_service.h"

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
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcWallpaperService* GetForBrowserContext(
      content::BrowserContext* context);

  ArcWallpaperService(content::BrowserContext* context,
                      ArcBridgeService* bridge_service);
  ~ArcWallpaperService() override;

  // mojom::WallpaperHost overrides.
  void SetWallpaper(const std::vector<uint8_t>& data,
                    int32_t wallpaper_id) override;
  void SetDefaultWallpaper() override;
  void GetWallpaper(GetWallpaperCallback callback) override;

  class DecodeRequestSender {
   public:
    virtual ~DecodeRequestSender();

    // Decodes image |data| and notifies the result to |request|.
    virtual void SendDecodeRequest(ImageDecoder::ImageRequest* request,
                                   const std::vector<uint8_t>& data) = 0;
  };

  // Replace a way to decode images for unittests. Originally it uses
  // ImageDecoder which communicates with the external process.
  void SetDecodeRequestSenderForTesting(
      std::unique_ptr<DecodeRequestSender> sender);

 private:
  friend class TestApi;
  class AndroidIdStore;
  class DecodeRequest;

  // Initiates a set wallpaper request to //ash.
  void OnWallpaperDecoded(const gfx::ImageSkia& image, int32_t android_id);

  // Notifies wallpaper change if we have wallpaper instance.
  void NotifyWallpaperChanged(int android_id);

  // Notifies wallpaper change of |android_id|, then notify wallpaper change of
  // -1 to reset wallpaper cache at Android side.
  void NotifyWallpaperChangedAndReset(int android_id);

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.
  std::unique_ptr<DecodeRequest> decode_request_;
  std::unique_ptr<DecodeRequestSender> decode_request_sender_;

  DISALLOW_COPY_AND_ASSIGN(ArcWallpaperService);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_WALLPAPER_ARC_WALLPAPER_SERVICE_H_
