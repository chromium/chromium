// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_RESTORE_INFORMED_RESTORE_APP_IMAGE_VIEW_H_
#define ASH_WM_WINDOW_RESTORE_INFORMED_RESTORE_APP_IMAGE_VIEW_H_

#include "ash/ash_export.h"
#include "base/scoped_observation.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "ui/views/controls/image_view.h"

namespace ash {

// TODO(hewer): Update description when default icon is added.
// Displays the icon of the given app if available, and observes the
// `AppRegistryCache` to update the icon after it has been marked as ready
// (installed). `ready_callback` is run after the given `app_id` has been
// marked as "ready" by the `AppRegistryCache`.
class ASH_EXPORT InformedRestoreAppImageView : public views::ImageView,
                                    public apps::AppRegistryCache::Observer {
  METADATA_HEADER(InformedRestoreAppImageView, views::ImageView)

 public:
  // Determines the styling of the view, based on where it is used.
  enum class Type { kItem, kScreenshot, kOverflow };

  InformedRestoreAppImageView(const std::string& app_id,
                   const Type type,
                   base::OnceClosure ready_callback);
  InformedRestoreAppImageView(const InformedRestoreAppImageView&) = delete;
  InformedRestoreAppImageView& operator=(const InformedRestoreAppImageView&)
      = delete;
  ~InformedRestoreAppImageView() override;

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

 private:
  // Sets `icon` as the image for the view.
  void GetIconCallback(const gfx::ImageSkia& icon);

  const std::string app_id_;
  const Type type_;

  // Called by `OnAppUpdate()` if the app is marked as "ready".
  base::OnceClosure ready_callback_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};

  base::WeakPtrFactory<InformedRestoreAppImageView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_RESTORE_INFORMED_RESTORE_APP_IMAGE_VIEW_H_
