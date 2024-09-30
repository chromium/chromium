// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_VIRTUAL_TRACKPAD_VIRTUAL_TRACKPAD_VIEW_H_
#define ASH_VIRTUAL_TRACKPAD_VIRTUAL_TRACKPAD_VIEW_H_

#include "ash/ash_export.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"

namespace views {
class LabelButton;
class Widget;
}  // namespace views

namespace ash {

class BlurredBackgroundShield;
class TrackpadInternalSurfaceView;

// The contents view of the widget which houses the virtual trackpad. Serves as
// the parent for the controls to modify our fake scrolls and the virtual
// trackpad view.
// TODO(b/288286805): Handle 2 finger scrolls.
class VirtualTrackpadView : public views::View {
  METADATA_HEADER(VirtualTrackpadView, views::View)

 public:
  VirtualTrackpadView();
  VirtualTrackpadView(const VirtualTrackpadView&) = delete;
  VirtualTrackpadView& operator=(const VirtualTrackpadView&) = delete;
  ~VirtualTrackpadView() override;

  // Toggles the visibility of a virtual trackpad Ui for emulating trackpad
  // gestures.
  static void Toggle();

  // views::View:
  void Layout(PassKey) override;

  static ASH_EXPORT views::Widget* GetWidgetForTesting();

 private:
  friend class VirtualTrackpadTest;

  // Updates the internal state for how many fingers to drag with. Right now, it
  // only supports 3 or 4 finger drags.
  void OnFingerButtonPressed(int num_fingers);

  void UpdateFingerButtonsColors();

  // Casted so we don't need to expose `TrackpadInternalSurfaceView`.
  ASH_EXPORT views::View* GetTrackpadViewForTesting();

  // Owned by views hierarchy.
  raw_ptr<views::BoxLayoutView> finger_buttons_panel_;

  // Keeps track of each `LabelButton` that is housed inside
  // `finger_buttons_panel_`. The key represents the number of fingers that the
  // `LabelButton` activates for future gestures. This map is ultimately used to
  // highlight the active button with a different color.
  base::flat_map<int, raw_ptr<views::LabelButton, CtnExperimental>>
      finger_buttons_;
  raw_ptr<TrackpadInternalSurfaceView> trackpad_view_ = nullptr;

  // Creates a new layer that blurs the background underneath the view layer.
  std::unique_ptr<BlurredBackgroundShield> blurred_background_;
};

}  // namespace ash

#endif  // ASH_VIRTUAL_TRACKPAD_VIRTUAL_TRACKPAD_VIEW_H_
