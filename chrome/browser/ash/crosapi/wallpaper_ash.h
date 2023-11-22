// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_WALLPAPER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_WALLPAPER_ASH_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/wallpaper.mojom.h"
#include "extensions/common/extension_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"

namespace crosapi {

// Ash implementation of the wallpaper extension API in Lacros.
class WallpaperAsh : public mojom::Wallpaper {
 public:
  WallpaperAsh();
  WallpaperAsh(const WallpaperAsh&) = delete;
  WallpaperAsh& operator=(const WallpaperAsh&) = delete;
  ~WallpaperAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::Wallpaper> receiver);

  // mojom::Wallpaper:
  // Implementation removed in M116.
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
                          const SkBitmap& bitmap);
  void SendErrorResult(const std::string& response);
  void SendSuccessResult(const std::vector<uint8_t>& thumbnail_data);

  mojo::ReceiverSet<mojom::Wallpaper> receivers_;
  // The ID of the extension making the current SetWallpaper() call.
  extensions::ExtensionId extension_id_;
  SetWallpaperCallback pending_callback_;
  data_decoder::DataDecoder data_decoder_;
  base::WeakPtrFactory<WallpaperAsh> weak_ptr_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_WALLPAPER_ASH_H_
