// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_ICON_LOADER_H_
#define CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_ICON_LOADER_H_

#include "base/callback_forward.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/image/image_skia.h"

namespace base {
class FilePath;
}

namespace ash {

// Loads locally stored icon data and decodes it.
class KioskAppIconLoader {
 public:
  class Delegate {
   public:
    virtual void OnIconLoadSuccess(const gfx::ImageSkia& icon) = 0;
    virtual void OnIconLoadFailure() = 0;

   protected:
    virtual ~Delegate() = default;
  };

  using ResultCallback =
      base::OnceCallback<void(absl::optional<gfx::ImageSkia> result)>;

  explicit KioskAppIconLoader(Delegate* delegate);
  KioskAppIconLoader(const KioskAppIconLoader&) = delete;
  KioskAppIconLoader& operator=(const KioskAppIconLoader&) = delete;
  ~KioskAppIconLoader();

  void Start(const base::FilePath& icon_path);

 private:
  void OnImageDecodingFinished(absl::optional<gfx::ImageSkia> result);

  // Delegate always lives longer than this class as it's owned by delegate.
  Delegate* const delegate_;

  gfx::ImageSkia icon_;

  base::WeakPtrFactory<KioskAppIconLoader> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_ICON_LOADER_H_
