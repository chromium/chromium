// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_AMBIENT_BACKGROUND_IMAGE_VIEW_H_
#define ASH_AMBIENT_UI_AMBIENT_BACKGROUND_IMAGE_VIEW_H_

#include <string>

#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ash/ash_export.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

class GlanceableInfoView;

// AmbientBackgroundImageView--------------------------------------------------
// A custom ImageView to display photo image and details information on ambient.
// It also handles specific mouse/gesture events to dismiss ambient when user
// interacts with the background photos.
class ASH_EXPORT AmbientBackgroundImageView : public views::View {
 public:
  METADATA_HEADER(AmbientBackgroundImageView);

  explicit AmbientBackgroundImageView(AmbientViewDelegate* delegate);
  AmbientBackgroundImageView(const AmbientBackgroundImageView&) = delete;
  AmbientBackgroundImageView& operator=(AmbientBackgroundImageView&) = delete;
  ~AmbientBackgroundImageView() override;

  // views::View
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Updates the display image.
  void UpdateImage(const gfx::ImageSkia& img);

  // Updates the details for the currently displayed image.
  void UpdateImageDetails(const base::string16& details);

  const gfx::ImageSkia& GetCurrentImage();

  gfx::Rect GetCurrentImageBoundsForTesting() const;

 private:
  void InitLayout();

  void UpdateGlanceableInfoPosition();

  // Owned by |AmbientController| and should always outlive |this|.
  AmbientViewDelegate* delegate_ = nullptr;

  // View to display the current image on ambient. Owned by the view hierarchy.
  views::ImageView* image_view_ = nullptr;

  GlanceableInfoView* glanceable_info_view_ = nullptr;

  // Label to show details text, i.e. attribution, to be displayed for the
  // current image. Owned by the view hierarchy.
  views::Label* details_label_ = nullptr;
};
}  // namespace ash

#endif  // ASH_AMBIENT_UI_AMBIENT_BACKGROUND_IMAGE_VIEW_H_
