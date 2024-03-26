// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_RESTORE_PINE_APP_IMAGE_VIEW_H_
#define ASH_WM_WINDOW_RESTORE_PINE_APP_IMAGE_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/controls/image_view.h"

namespace ash {

// TODO(hewer): Update description when default icon and observer are added.
// Displays the icon of the given app if available.
class ASH_EXPORT PineAppImageView : public views::ImageView {
  METADATA_HEADER(PineAppImageView, views::ImageView)

 public:
  PineAppImageView(const std::string& app_id, bool inside_screenshot);
  PineAppImageView(const PineAppImageView&) = delete;
  PineAppImageView& operator=(const PineAppImageView&) = delete;
  ~PineAppImageView() override;

 private:
  void GetIconCallback(const gfx::ImageSkia& icon);

  base::WeakPtrFactory<PineAppImageView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_RESTORE_PINE_APP_IMAGE_VIEW_H_
