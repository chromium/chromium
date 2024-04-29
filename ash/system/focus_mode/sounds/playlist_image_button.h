// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_SOUNDS_PLAYLIST_IMAGE_BUTTON_H_
#define ASH_SYSTEM_FOCUS_MODE_SOUNDS_PLAYLIST_IMAGE_BUTTON_H_

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/delegating_layout_manager.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace views {
class AnimatedImageView;
class ImageView;
}  // namespace views

namespace ash {

class ASH_EXPORT PlaylistImageButton : public views::Button,
                                       public views::LayoutDelegate {
  METADATA_HEADER(PlaylistImageButton, views::Button)

 public:
  PlaylistImageButton();
  PlaylistImageButton(const PlaylistImageButton&) = delete;
  PlaylistImageButton& operator=(const PlaylistImageButton&) = delete;
  ~PlaylistImageButton() override;

  // views::Button:
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

  // views::LayoutDelegate:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;

  // Called when the playback state of the media is changed to the play/stop
  // state.
  void SetIsPlaying(bool is_playing);

  // Replaces the `image_view_` with a new image and the press callback for a
  // new playlist.
  void UpdateContents(const gfx::ImageSkia& image, PressedCallback callback);

 private:
  // views::Button:
  void OnSetTooltipText(const std::u16string& tooltip_text) override;

  void UpdateVisibility();

  bool is_playing_ = false;
  raw_ptr<views::ImageView> image_view_ = nullptr;
  raw_ptr<views::ImageView> media_action_icon_ = nullptr;
  raw_ptr<views::ImageView> selected_curvycutout_icon_ = nullptr;
  raw_ptr<views::AnimatedImageView> lottie_animation_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_SOUNDS_PLAYLIST_IMAGE_BUTTON_H_
