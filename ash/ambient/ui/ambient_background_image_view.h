// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_AMBIENT_BACKGROUND_IMAGE_VIEW_H_
#define ASH_AMBIENT_UI_AMBIENT_BACKGROUND_IMAGE_VIEW_H_

#include <string>

#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ash/ash_export.h"
#include "base/scoped_observer.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

class GlanceableInfoView;
class MediaStringView;

// AmbientBackgroundImageView--------------------------------------------------
// A custom ImageView to display photo image and details information on ambient.
// It also handles specific mouse/gesture events to dismiss ambient when user
// interacts with the background photos.
class ASH_EXPORT AmbientBackgroundImageView : public views::View,
                                              public views::ViewObserver {
 public:
  METADATA_HEADER(AmbientBackgroundImageView);

  explicit AmbientBackgroundImageView(AmbientViewDelegate* delegate);
  AmbientBackgroundImageView(const AmbientBackgroundImageView&) = delete;
  AmbientBackgroundImageView& operator=(AmbientBackgroundImageView&) = delete;
  ~AmbientBackgroundImageView() override;

  // views::View:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;

  // Updates the display images.
  void UpdateImage(const gfx::ImageSkia& image,
                   const gfx::ImageSkia& related_image);

  // Updates the details for the currently displayed image.
  void UpdateImageDetails(const base::string16& details);

  const gfx::ImageSkia& GetCurrentImage();

  gfx::Rect GetImageBoundsForTesting() const;
  gfx::Rect GetRelatedImageBoundsForTesting() const;
  void ResetRelatedImageForTesting();

 private:
  void InitLayout();

  void UpdateGlanceableInfoPosition();

  bool UpdateRelatedImageViewVisibility();
  void SetResizedImage(views::ImageView* image_view,
                       const gfx::ImageSkia& image_unscaled);

  // Whether the device is in landscape orientation.
  bool IsLandscapeOrientation() const;

  bool HasPairedImages() const;

  // Owned by |AmbientController| and should always outlive |this|.
  AmbientViewDelegate* delegate_ = nullptr;

  // View to display current image(s) on ambient. Owned by the view hierarchy.
  views::View* image_container_ = nullptr;
  views::ImageView* image_view_ = nullptr;
  views::ImageView* related_image_view_ = nullptr;

  // The unscaled images used for scaling and displaying in different bounds.
  gfx::ImageSkia image_unscaled_;
  gfx::ImageSkia related_image_unscaled_;

  GlanceableInfoView* glanceable_info_view_ = nullptr;

  // Label to show details text, i.e. attribution, to be displayed for the
  // current image. Owned by the view hierarchy.
  views::Label* details_label_ = nullptr;

  MediaStringView* media_string_view_ = nullptr;

  ScopedObserver<views::View, views::ViewObserver> observed_views_{this};
};
}  // namespace ash

#endif  // ASH_AMBIENT_UI_AMBIENT_BACKGROUND_IMAGE_VIEW_H_
