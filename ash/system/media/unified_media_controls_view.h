// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MEDIA_UNIFIED_MEDIA_CONTROLS_VIEW_H_
#define ASH_SYSTEM_MEDIA_UNIFIED_MEDIA_CONTROLS_VIEW_H_

#include "ash/ash_export.h"
#include "base/containers/flat_set.h"
#include "base/optional.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace ash {

class UnifiedMediaControlsController;

// Media controls view displayed in quick settings.
class ASH_EXPORT UnifiedMediaControlsView : public views::Button {
 public:
  explicit UnifiedMediaControlsView(UnifiedMediaControlsController* controller);
  ~UnifiedMediaControlsView() override = default;

  void SetIsPlaying(bool playing);
  void SetArtwork(base::Optional<gfx::ImageSkia> artwork);
  void SetTitle(const base::string16& title);
  void SetArtist(const base::string16& artist);
  void UpdateActionButtonAvailability(
      const base::flat_set<media_session::mojom::MediaSessionAction>&
          enabled_actions);

  // views::Button:
  void OnThemeChanged() override;

  // Show an empty state representing no media is playing.
  void ShowEmptyState();

  // Called when receiving new media session, update controls to normal state
  // if necessary.
  void OnNewMediaSession();

  views::ImageView* artwork_view() { return artwork_view_; }

 private:
  friend class UnifiedMediaControlsControllerTest;

  class MediaActionButton : public views::ImageButton {
   public:
    MediaActionButton(UnifiedMediaControlsController* controller,
                      media_session::mojom::MediaSessionAction action,
                      const base::string16& accessible_name);
    ~MediaActionButton() override = default;

    void SetAction(media_session::mojom::MediaSessionAction action,
                   const base::string16& accessible_name);

    // views::ImageButton:
    std::unique_ptr<views::InkDrop> CreateInkDrop() override;
    std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
        const override;
    std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;
    void OnThemeChanged() override;

   private:
    void UpdateVectorIcon();

    // Action that can be taken on the media through the button, it can be paly,
    // pause or stop the media etc. See MediaSessionAction for all the actions.
    media_session::mojom::MediaSessionAction action_;
  };

  SkPath GetArtworkClipPath();

  UnifiedMediaControlsController* const controller_ = nullptr;

  views::ImageView* artwork_view_ = nullptr;
  views::ImageView* drop_down_icon_ = nullptr;
  views::Label* title_label_ = nullptr;
  views::Label* artist_label_ = nullptr;
  MediaActionButton* play_pause_button_ = nullptr;
  views::View* button_row_ = nullptr;

  bool is_in_empty_state_ = false;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MEDIA_UNIFIED_MEDIA_CONTROLS_VIEW_H_
