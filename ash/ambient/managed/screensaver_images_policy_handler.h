// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_MANAGED_SCREENSAVER_IMAGES_POLICY_HANDLER_H_
#define ASH_AMBIENT_MANAGED_SCREENSAVER_IMAGES_POLICY_HANDLER_H_

#include <memory>

#include "ash/ambient/managed/screensaver_image_downloader.h"
#include "ash/ash_export.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/prefs/pref_change_registrar.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {

// Observes the policy that provides image sources for the managed screensaver
// feature in order to download and cache the images.
class ASH_EXPORT ScreensaverImagesPolicyHandler {
 public:
  using ScreensaverImagesRepeatingCallback =
      base::RepeatingCallback<void(const std::vector<base::FilePath>& images)>;

  enum HandlerType { kSignin, kUser, kManagedGuest };

  static std::unique_ptr<ScreensaverImagesPolicyHandler> Create(
      PrefService* pref_service);

  static void RegisterPrefs(PrefRegistrySimple* registry);

  ScreensaverImagesPolicyHandler(PrefService* pref_service, HandlerType state);
  ScreensaverImagesPolicyHandler(const ScreensaverImagesPolicyHandler&) =
      delete;
  ScreensaverImagesPolicyHandler& operator=(
      const ScreensaverImagesPolicyHandler&) = delete;

  ~ScreensaverImagesPolicyHandler();

  std::vector<base::FilePath> GetScreensaverImages();
  void SetScreensaverImagesUpdatedCallback(
      ScreensaverImagesRepeatingCallback callback);

  // Used for setting images in tests.
  void SetImagesForTesting(const std::vector<base::FilePath>& images);

 private:
  friend class ScreensaverImagesPolicyHandlerTest;

  void OnAmbientModeManagedScreensaverImagesPrefChanged();

  void OnDownloadedImageListUpdated(const std::vector<base::FilePath>& images);

  bool IsManagedScreensaverDisabledByPolicy();

  raw_ptr<PrefService> pref_service_ = nullptr;

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  std::unique_ptr<ScreensaverImageDownloader> image_downloader_;

  ScreensaverImagesRepeatingCallback on_images_updated_callback_;
  base::WeakPtrFactory<ScreensaverImagesPolicyHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_AMBIENT_MANAGED_SCREENSAVER_IMAGES_POLICY_HANDLER_H_
