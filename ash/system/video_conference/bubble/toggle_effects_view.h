// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_TOGGLE_EFFECTS_VIEW_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_TOGGLE_EFFECTS_VIEW_H_

#include "ash/system/video_conference/effects/video_conference_tray_effects_manager.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace ui {
class Event;
}  // namespace ui

namespace views {
class FlexLayout;
class ImageView;
class Label;
}  // namespace views

namespace ash {
enum class VcEffectId;
class VideoConferenceTrayController;

namespace video_conference {

// A single toggle button for a video conference effect, combined with a text
// label. WARNING: `callback` provided must not destroy the button or the bubble
// (i.e. close the bubble) as it would result in a crash in `OnButtonClicked()`.
class ToggleEffectsButton : public views::Button,
                            public VideoConferenceTrayEffectsManager::Observer {
  METADATA_HEADER(ToggleEffectsButton, views::Button)

 public:
  ToggleEffectsButton(views::Button::PressedCallback callback,
                      const gfx::VectorIcon* vector_icon,
                      bool toggle_state,
                      const std::u16string& label_text,
                      const int accessible_name_id,
                      std::optional<int> container_id,
                      const VcEffectId effect_id,
                      int num_button_per_row);

  ToggleEffectsButton(const ToggleEffectsButton&) = delete;
  ToggleEffectsButton& operator=(const ToggleEffectsButton&) = delete;

  ~ToggleEffectsButton() override;

  // VideoConferenceTrayEffectsManager::Observer:
  void OnEffectChanged(VcEffectId effect_id, bool is_on) override;

  views::FlexLayout* layout() { return layout_; }

  views::ImageView* icon() { return icon_; }

 private:
  // Callback for clicking the button.
  void OnButtonClicked(const ui::Event& event);

  // Update the color of icon/label and background.
  void UpdateColorsAndBackground();

  // Update the tooltip text.
  void UpdateTooltip();

  views::Button::PressedCallback callback_;

  // Indicates the toggled state of the button.
  bool toggled_ = false;

  // The effect id associated to the effect of this button.
  const VcEffectId effect_id_;

  // Owned by the views hierarchy.
  raw_ptr<views::ImageView> icon_ = nullptr;
  raw_ptr<views::Label> label_ = nullptr;

  raw_ptr<const gfx::VectorIcon> vector_icon_;
  const int accessible_name_id_;

  raw_ptr<views::FlexLayout> layout_ = nullptr;

  base::WeakPtrFactory<ToggleEffectsButton> weak_ptr_factory_{this};
};

// The toggle effects view, that resides in the video conference bubble. It
// functions as a "factory" that constructs and hosts rows of buttons, with each
// button managing the on/off state for an individual effect. The buttons are
// constructed from effects data gathered from `VcEffectsDelegate` objects that
// host the individual effects and are registered with the
// `VideoConferenceTrayEffectsManager`, which is in turn owned by the passed-in
// controller.
class ToggleEffectsView : public views::View {
  METADATA_HEADER(ToggleEffectsView, views::View)

 public:
  explicit ToggleEffectsView(VideoConferenceTrayController* controller);
  ToggleEffectsView(const ToggleEffectsView&) = delete;
  ToggleEffectsView& operator=(const ToggleEffectsView&) = delete;
  ~ToggleEffectsView() override = default;
};

}  // namespace video_conference

}  // namespace ash

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_TOGGLE_EFFECTS_VIEW_H_
