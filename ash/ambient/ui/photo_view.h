// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_PHOTO_VIEW_H_
#define ASH_AMBIENT_UI_PHOTO_VIEW_H_

#include "ash/ambient/model/photo_model_observer.h"
#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
}  // namespace views

namespace ash {

class AmbientController;

// View to display photos in ambient mode.
class ASH_EXPORT PhotoView : public views::View, public PhotoModelObserver {
 public:
  explicit PhotoView(AmbientController* ambient_controller);
  ~PhotoView() override;

  // views::View:
  const char* GetClassName() const override;
  void AddedToWidget() override;

  // PhotoModelObserver:
  void OnImagesChanged() override;

 private:
  void Init();
  void UpdateImages();
  void StartSlideAnimation();
  bool CanAnimate() const;

  AmbientController* ambient_controller_ = nullptr;

  // Image containers. Used for layer animation.
  views::ImageView* image_view_prev_ = nullptr;  // Owned by view hierarchy.
  views::ImageView* image_view_curr_ = nullptr;  // Owned by view hierarchy.
  views::ImageView* image_view_next_ = nullptr;  // Owned by view hierarchy.

  DISALLOW_COPY_AND_ASSIGN(PhotoView);
};

}  // namespace ash

#endif  // ASH_AMBIENT_UI_PHOTO_VIEW_H_
