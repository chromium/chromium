// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_SET_VALUE_EFFECTS_VIEW_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_SET_VALUE_EFFECTS_VIEW_H_

#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class TabSlider;
class VcHostedEffect;
class VideoConferenceTrayController;

namespace video_conference {

// The slider that allows user to pick a value for an effect of type
// `VcEffectType::kSetValue`. This view consists a label (for the effect name)
// and a tab slider that allows the user to select from one of several integer
// values.
class SetValueEffectSlider : public views::View {
  METADATA_HEADER(SetValueEffectSlider, views::View)

 public:
  SetValueEffectSlider(VideoConferenceTrayController* controller,
                       const VcHostedEffect* effect);

  SetValueEffectSlider(const SetValueEffectSlider&) = delete;
  SetValueEffectSlider& operator=(const SetValueEffectSlider&) = delete;

  ~SetValueEffectSlider() override = default;

  TabSlider* tab_slider() { return tab_slider_; }

  VcEffectId effect_id() { return effect_id_; }

 private:
  // Owned by the views hierarchy.
  raw_ptr<TabSlider> tab_slider_ = nullptr;

  // The id associated with this effect.
  const VcEffectId effect_id_;
};

// The set-value effects view that resides in the video conference bubble,
// containing all the `SetValueEffectSlider` views in this bubble.
class SetValueEffectsView : public views::View {
  METADATA_HEADER(SetValueEffectsView, views::View)

 public:
  explicit SetValueEffectsView(VideoConferenceTrayController* controller);
  SetValueEffectsView(const SetValueEffectsView&) = delete;
  SetValueEffectsView& operator=(const SetValueEffectsView&) = delete;
  ~SetValueEffectsView() override = default;
};

}  // namespace video_conference

}  // namespace ash

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_SET_VALUE_EFFECTS_VIEW_H_
