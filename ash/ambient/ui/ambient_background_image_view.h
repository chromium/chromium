// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_AMBIENT_BACKGROUND_IMAGE_VIEW_H_
#define ASH_AMBIENT_UI_AMBIENT_BACKGROUND_IMAGE_VIEW_H_

#include <string>

#include "ash/ambient/ui/ambient_slideshow_peripheral_ui.h"
#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace ash {

// AmbientBackgroundImageView--------------------------------------------------
// A custom ImageView to display photo image and details information on ambient.
// It also handles specific mouse/gesture events to dismiss ambient when user
// interacts with the background photos.
class ASH_EXPORT AmbientBackgroundImageView : public views::View,
                                              public views::ViewObserver {
  METADATA_HEADER(AmbientBackgroundImageView, views::View)

 public:
  explicit AmbientBackgroundImageView(AmbientViewDelegate* delegate);
  AmbientBackgroundImageView(const AmbientBackgroundImageView&) = delete;
  AmbientBackgroundImageView& operator=(const AmbientBackgroundImageView&) =
      delete;
  ~AmbientBackgroundImageView() override;

  // views::View:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;

  // Updates the display images.
  void UpdateImage(const gfx::ImageSkia& image,
                   const gfx::ImageSkia& related_image,
                   bool is_portrait,
                   ::ambient::TopicType type);

  // Updates the details for the currently displayed image(s).
  void UpdateImageDetails(const std::u16string& details,
                          const std::u16string& related_details);

  // Shows/Hides the peripheral ui.
  void SetPeripheralUiVisibility(bool visible);

  void SetForceResizeToFit(bool force_resize_to_fit);

  gfx::ImageSkia GetCurrentImage();

  gfx::Rect GetImageBoundsInScreenForTesting() const;
  gfx::Rect GetRelatedImageBoundsInScreenForTesting() const;
  void ResetRelatedImageForTesting();

 private:
  void InitLayout();

  void UpdateLayout();
  bool UpdateRelatedImageViewVisibility();
  void SetResizedImage(views::ImageView* image_view,
                       const gfx::ImageSkia& image_unscaled);

  // When show paired images:
  // 1. The device is in landscape mode and the images are portrait.
  // 2. The device is in portrait mode and the images are landscape.
  bool MustShowPairs() const;

  bool HasPairedImages() const;

  // Owned by |AmbientController| and should always outlive |this|.
  raw_ptr<AmbientViewDelegate> delegate_ = nullptr;

  // View to display current image(s) on ambient. Owned by the view hierarchy.
  raw_ptr<views::View> image_container_ = nullptr;
  raw_ptr<views::FlexLayout> image_layout_ = nullptr;
  raw_ptr<views::ImageView> image_view_ = nullptr;
  raw_ptr<views::ImageView> related_image_view_ = nullptr;

  // The unscaled images used for scaling and displaying in different bounds.
  gfx::ImageSkia image_unscaled_;
  gfx::ImageSkia related_image_unscaled_;

  std::u16string details_;
  std::u16string related_details_;

  bool is_portrait_ = false;

  // Flag that changes the resize behavior such that full image is always shown
  // without any cropping. False by default.
  bool force_resize_to_fit_ = false;

  ::ambient::TopicType topic_type_ = ::ambient::TopicType::kOther;

  raw_ptr<AmbientSlideshowPeripheralUi> ambient_peripheral_ui_ = nullptr;

  base::ScopedMultiSourceObservation<views::View, views::ViewObserver>
      observed_views_{this};
};
}  // namespace ash

#endif  // ASH_AMBIENT_UI_AMBIENT_BACKGROUND_IMAGE_VIEW_H_
