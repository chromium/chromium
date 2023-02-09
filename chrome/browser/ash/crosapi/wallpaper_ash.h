// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_WALLPAPER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_WALLPAPER_ASH_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/wallpaper.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"

namespace crosapi {

class WallpaperAsh : public mojom::Wallpaper {
 public:
  WallpaperAsh();
  WallpaperAsh(const WallpaperAsh&) = delete;
  WallpaperAsh& operator=(const WallpaperAsh&) = delete;
  ~WallpaperAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::Wallpaper> receiver);

  // mojom::Wallpaper:
  // Use SetWallpaper instead of SetWallpaperDeprecated. SetWallpaper has more
  // comprehensive error responses when failing to download, decode, or set
  // wallpapers.
  // TODO(b/258819982): Remove in M115.
  void SetWallpaperDeprecated(mojom::WallpaperSettingsPtr wallpaper_settings,
                              const std::string& extension_id,
                              const std::string& extension_name,
                              SetWallpaperDeprecatedCallback callback) override;
  void SetWallpaper(mojom::WallpaperSettingsPtr wallpaper_settings,
                    const std::string& extension_id,
                    const std::string& extension_name,
                    SetWallpaperCallback callback) override;

 private:
  void OnWallpaperDecoded(mojom::WallpaperSettingsPtr wallpaper_settings,
                          const std::string& extension_id,
                          const std::string& extension_name,
                          const SkBitmap& bitmap);
  void SendErrorResult(const std::string& response);
  void SendSuccessResult(const std::vector<uint8_t>& thumbnail_data);

  mojo::ReceiverSet<mojom::Wallpaper> receivers_;
  // TODO(b/258819982): Remove in M115.
  SetWallpaperDeprecatedCallback deprecated_pending_callback_;
  SetWallpaperCallback pending_callback_;
  data_decoder::DataDecoder data_decoder_;
  base::WeakPtrFactory<WallpaperAsh> weak_ptr_factory_{this};
};

}  // namespace crosapi

#endif
