// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_AMBIENT_BACKGROUND_IMAGE_VIEW_H_
#define ASH_AMBIENT_UI_AMBIENT_BACKGROUND_IMAGE_VIEW_H_

#include <string>

#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ash/ambient/ui/media_string_view.h"
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

class AmbientInfoView;
class JitterCalculator;
class MediaStringView;

// AmbientBackgroundImageView--------------------------------------------------
// A custom ImageView to display photo image and details information on ambient.
// It also handles specific mouse/gesture events to dismiss ambient when user
// interacts with the background photos.
class ASH_EXPORT AmbientBackgroundImageView : public views::View,
                                              public views::ViewObserver,
                                              public MediaStringView::Delegate {
 public:
  METADATA_HEADER(AmbientBackgroundImageView);

  AmbientBackgroundImageView(
      AmbientViewDelegate* delegate,
      JitterCalculator* glanceable_info_jitter_calculator);
  AmbientBackgroundImageView(const AmbientBackgroundImageView&) = delete;
  AmbientBackgroundImageView& operator=(const AmbientBackgroundImageView&) =
      delete;
  ~AmbientBackgroundImageView() override;

  // views::View:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;

  // MediaStringView::Delegate:
  MediaStringView::Settings GetSettings() override;

  // Updates the display images.
  void UpdateImage(const gfx::ImageSkia& image,
                   const gfx::ImageSkia& related_image,
                   bool is_portrait,
                   ::ambient::TopicType type);

  // Updates the details for the currently displayed image(s).
  void UpdateImageDetails(const std::u16string& details,
                          const std::u16string& related_details);

  gfx::ImageSkia GetCurrentImage();

  gfx::Rect GetImageBoundsInScreenForTesting() const;
  gfx::Rect GetRelatedImageBoundsInScreenForTesting() const;
  void ResetRelatedImageForTesting();

 private:
  void InitLayout();

  void UpdateGlanceableInfoPosition();

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
  AmbientViewDelegate* delegate_ = nullptr;

  const base::raw_ptr<JitterCalculator> glanceable_info_jitter_calculator_;

  // View to display current image(s) on ambient. Owned by the view hierarchy.
  views::View* image_container_ = nullptr;
  views::FlexLayout* image_layout_ = nullptr;
  views::ImageView* image_view_ = nullptr;
  views::ImageView* related_image_view_ = nullptr;

  // The unscaled images used for scaling and displaying in different bounds.
  gfx::ImageSkia image_unscaled_;
  gfx::ImageSkia related_image_unscaled_;

  std::u16string details_;
  std::u16string related_details_;

  bool is_portrait_ = false;

  ::ambient::TopicType topic_type_ = ::ambient::TopicType::kOther;

  AmbientInfoView* ambient_info_view_ = nullptr;

  MediaStringView* media_string_view_ = nullptr;

  base::ScopedMultiSourceObservation<views::View, views::ViewObserver>
      observed_views_{this};
};
}  // namespace ash

#endif  // ASH_AMBIENT_UI_AMBIENT_BACKGROUND_IMAGE_VIEW_H_
