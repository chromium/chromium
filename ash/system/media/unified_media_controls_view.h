// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MEDIA_UNIFIED_MEDIA_CONTROLS_VIEW_H_
#define ASH_SYSTEM_MEDIA_UNIFIED_MEDIA_CONTROLS_VIEW_H_

#include "ash/ash_export.h"
#include "ash/style/icon_button.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/views/controls/button/button.h"

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
  void SetArtwork(absl::optional<gfx::ImageSkia> artwork);
  void SetTitle(const std::u16string& title);
  void SetArtist(const std::u16string& artist);
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

  class MediaActionButton : public IconButton {
   public:
    MediaActionButton(UnifiedMediaControlsController* controller,
                      media_session::mojom::MediaSessionAction action,
                      int accessible_name_id);
    MediaActionButton(const MediaActionButton&) = delete;
    MediaActionButton& operator=(const MediaActionButton&) = delete;
    ~MediaActionButton() override = default;

    void SetAction(media_session::mojom::MediaSessionAction action,
                   const std::u16string& accessible_name);

   private:
    // Action that can be taken on the media through the button, it can be paly,
    // pause or stop the media etc. See MediaSessionAction for all the actions.
    media_session::mojom::MediaSessionAction action_;
  };

  SkPath GetArtworkClipPath();

  const raw_ptr<UnifiedMediaControlsController, ExperimentalAsh> controller_ =
      nullptr;

  raw_ptr<views::ImageView, ExperimentalAsh> artwork_view_ = nullptr;
  raw_ptr<views::ImageView, ExperimentalAsh> drop_down_icon_ = nullptr;
  raw_ptr<views::Label, ExperimentalAsh> title_label_ = nullptr;
  raw_ptr<views::Label, ExperimentalAsh> artist_label_ = nullptr;
  raw_ptr<MediaActionButton, ExperimentalAsh> play_pause_button_ = nullptr;
  raw_ptr<views::View, ExperimentalAsh> button_row_ = nullptr;

  bool is_in_empty_state_ = false;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MEDIA_UNIFIED_MEDIA_CONTROLS_VIEW_H_
