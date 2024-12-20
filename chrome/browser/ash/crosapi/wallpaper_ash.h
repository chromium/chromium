// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_WALLPAPER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_WALLPAPER_ASH_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/wallpaper.mojom.h"
#include "extensions/common/extension_id.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"

namespace crosapi {

// Ash implementation of the wallpaper extension API.
class WallpaperAsh {
 public:
  WallpaperAsh();
  WallpaperAsh(const WallpaperAsh&) = delete;
  WallpaperAsh& operator=(const WallpaperAsh&) = delete;
  ~WallpaperAsh();

  void SetWallpaper(
      mojom::WallpaperSettingsPtr wallpaper_settings,
      const std::string& extension_id,
      const std::string& extension_name,
      base::OnceCallback<void(mojom::SetWallpaperResultPtr)> callback);

 private:
  void OnWallpaperDecoded(mojom::WallpaperSettingsPtr wallpaper_settings,
                          const SkBitmap& bitmap);
  void SendErrorResult(const std::string& response);
  void SendSuccessResult(const std::vector<uint8_t>& thumbnail_data);

  // The ID of the extension making the current SetWallpaper() call.
  extensions::ExtensionId extension_id_;
  base::OnceCallback<void(mojom::SetWallpaperResultPtr)> pending_callback_;
  data_decoder::DataDecoder data_decoder_;
  base::WeakPtrFactory<WallpaperAsh> weak_ptr_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_WALLPAPER_ASH_H_
