// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/video_conference_bubble.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "media/capture/video/chromeos/mojom/effects_pipeline.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/flex_layout.h"

namespace ash {

namespace {

// NOTE: The buttons/switches here are temporary, for testing purposes only. The
// real UI elements will be added when the underlying work to produce them is
// complete. See b/253273036 (styled radio switch) and b/253249205 (styled
// toggle button).
class VideoConferenceBackgroundBlurRadioSwitch : public views::View {
 public:
  VideoConferenceBackgroundBlurRadioSwitch() {
    const int kBackgroundBlurRadioGroupId = 1;

    views::FlexLayout* layout =
        SetLayoutManager(std::make_unique<views::FlexLayout>());
    layout->SetOrientation(views::LayoutOrientation::kHorizontal);
    layout->SetMainAxisAlignment(views::LayoutAlignment::kCenter);
    layout->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);

    AddChildView(std::make_unique<views::Label>(u"Background Blur"));

    std::unique_ptr<views::RadioButton> off_button =
        std::make_unique<views::RadioButton>(u"Off",
                                             kBackgroundBlurRadioGroupId);
    off_button->SetCallback(base::BindRepeating(
        &VideoConferenceBackgroundBlurRadioSwitch::OnLevelSelected,
        base::Unretained(this), -1));
    AddChildView(std::move(off_button));

    std::unique_ptr<views::RadioButton> lowest_button =
        std::make_unique<views::RadioButton>(u"Lowest",
                                             kBackgroundBlurRadioGroupId);
    lowest_button->SetCallback(base::BindRepeating(
        &VideoConferenceBackgroundBlurRadioSwitch::OnLevelSelected,
        base::Unretained(this), (int)cros::mojom::BlurLevel::kLowest));
    AddChildView(std::move(lowest_button));

    std::unique_ptr<views::RadioButton> light_button =
        std::make_unique<views::RadioButton>(u"Light",
                                             kBackgroundBlurRadioGroupId);
    light_button->SetCallback(base::BindRepeating(
        &VideoConferenceBackgroundBlurRadioSwitch::OnLevelSelected,
        base::Unretained(this), (int)cros::mojom::BlurLevel::kLight));
    AddChildView(std::move(light_button));

    std::unique_ptr<views::RadioButton> medium_button =
        std::make_unique<views::RadioButton>(u"Medium",
                                             kBackgroundBlurRadioGroupId);
    medium_button->SetCallback(base::BindRepeating(
        &VideoConferenceBackgroundBlurRadioSwitch::OnLevelSelected,
        base::Unretained(this), (int)cros::mojom::BlurLevel::kMedium));
    AddChildView(std::move(medium_button));

    std::unique_ptr<views::RadioButton> heavy_button =
        std::make_unique<views::RadioButton>(u"Heavy",
                                             kBackgroundBlurRadioGroupId);
    heavy_button->SetCallback(base::BindRepeating(
        &VideoConferenceBackgroundBlurRadioSwitch::OnLevelSelected,
        base::Unretained(this), (int)cros::mojom::BlurLevel::kHeavy));
    AddChildView(std::move(heavy_button));

    std::unique_ptr<views::RadioButton> maximum_button =
        std::make_unique<views::RadioButton>(u"Maximum",
                                             kBackgroundBlurRadioGroupId);
    maximum_button->SetCallback(base::BindRepeating(
        &VideoConferenceBackgroundBlurRadioSwitch::OnLevelSelected,
        base::Unretained(this), (int)cros::mojom::BlurLevel::kMaximum));
    AddChildView(std::move(maximum_button));
  }

  VideoConferenceBackgroundBlurRadioSwitch(
      const VideoConferenceBackgroundBlurRadioSwitch&) = delete;
  VideoConferenceBackgroundBlurRadioSwitch& operator=(
      const VideoConferenceBackgroundBlurRadioSwitch&) = delete;
  ~VideoConferenceBackgroundBlurRadioSwitch() override = default;

  // Callback that's invoked when the user selects (presses) one of the radio
  // buttons.
  void OnLevelSelected(int level) {}
};

}  // namespace

VideoConferenceBubbleView::LabeledToggleButton::LabeledToggleButton(
    views::Button::PressedCallback callback,
    const std::u16string& effect_name) {
  views::FlexLayout* layout =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kHorizontal);
  layout->SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  layout->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);

  AddChildView(std::make_unique<views::Label>(effect_name));

  std::unique_ptr<views::ToggleButton> button =
      std::make_unique<views::ToggleButton>(callback);
  button->SetAccessibleName(effect_name);
  button_ = AddChildView(std::move(button));
}

bool VideoConferenceBubbleView::LabeledToggleButton::GetIsOn() const {
  return button_->GetIsOn();
}

// NOTE: The buttons/switches here are temporary, for testing purposes only. The
// real UI elements will be added when the underlying work to produce them is
// complete. See b/253273036 (styled radio switch) and b/253249205(styled toggle
// button).
VideoConferenceBubbleView::VideoConferenceBubbleView(
    const InitParams& init_params)
    : TrayBubbleView(init_params) {
  // Toggle button for camera "background replace" effect.
  background_replace_button_ = AddChildView(std::make_unique<
                                            LabeledToggleButton>(
      base::BindRepeating(
          &VideoConferenceBubbleView::OnBackgroundReplaceToggleButtonPressed,
          base::Unretained(this)),
      u"Background Replace"));

  // Radio switch for camera "background blur" effect.
  AddChildView(std::make_unique<VideoConferenceBackgroundBlurRadioSwitch>());
}

void VideoConferenceBubbleView::OnBackgroundReplaceToggleButtonPressed() {
  // TODO (b/259585295) Use `background_replace_button_->GetIsOn()` to get the
  // toggle state of the button and enable/disable the actual effect.
}

}  // namespace ash
