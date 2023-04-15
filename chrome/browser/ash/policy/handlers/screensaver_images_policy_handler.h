// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_HANDLERS_SCREENSAVER_IMAGES_POLICY_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_HANDLERS_SCREENSAVER_IMAGES_POLICY_HANDLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/ambient/ambient_managed_photo_source.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/session/session_controller_impl.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/policy/handlers/screensaver_image_downloader.h"
#include "components/prefs/pref_change_registrar.h"

class PrefRegistrySimple;
class PrefService;

namespace policy {

// Observes the policy that provides image sources for the managed screensaver
// feature in order to download and cache the images.
class ASH_EXPORT ScreensaverImagesPolicyHandler
    : public ash::AmbientManagedPhotoSource,
      public ash::SessionObserver {
 public:
  static void RegisterPrefs(PrefRegistrySimple* registry);

  ScreensaverImagesPolicyHandler();
  ~ScreensaverImagesPolicyHandler() override;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // AmbientManagedPhotoSource overrides
  std::vector<base::FilePath> GetScreensaverImages() override;
  void SetScreensaverImagesUpdatedCallback(
      ScreensaverImagesRepeatingCallback callback) override;

 private:
  friend class ScreensaverImagesPolicyHandlerTest;

  void OnAmbientModeManagedScreensaverImagesPrefChanged();

  // Download completion handler.
  void OnDownloadJobCompleted(ScreensaverImageDownloadResult result,
                              absl::optional<base::FilePath> path);

  base::flat_set<base::FilePath> downloaded_images_;

  base::raw_ptr<PrefService> user_pref_service_ = nullptr;

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  std::unique_ptr<ScreensaverImageDownloader> image_downloader_;

  ScreensaverImagesRepeatingCallback on_images_updated_callback_;
  ash::ScopedSessionObserver scoped_session_observer_{this};
  base::WeakPtrFactory<ScreensaverImagesPolicyHandler> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_HANDLERS_SCREENSAVER_IMAGES_POLICY_HANDLER_H_
