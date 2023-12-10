// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_ICON_LOADER_H_
#define CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_ICON_LOADER_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/image/image_skia.h"

namespace base {
class FilePath;
}

namespace ash {

// Loads locally stored icon data and decodes it.
class KioskAppIconLoader {
 public:
  using ResultCallback =
      base::OnceCallback<void(std::optional<gfx::ImageSkia>)>;

  explicit KioskAppIconLoader(ResultCallback delegate);
  KioskAppIconLoader(const KioskAppIconLoader&) = delete;
  KioskAppIconLoader& operator=(const KioskAppIconLoader&) = delete;
  ~KioskAppIconLoader();

  void Start(const base::FilePath& icon_path);

 private:
  void OnImageDecodingFinished(std::optional<gfx::ImageSkia> result);

  ResultCallback callback_;

  base::WeakPtrFactory<KioskAppIconLoader> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_ICON_LOADER_H_
