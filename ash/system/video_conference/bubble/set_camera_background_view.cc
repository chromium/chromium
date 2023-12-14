// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/bubble/set_camera_background_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/video_conference/bubble/bubble_view_ids.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/box_layout.h"

namespace ash::video_conference {

namespace {

// Decides the margin for the `SetCameraBackgroundView`.
constexpr gfx::Insets kSetCameraBackgroundViewInsideBorderInsets =
    gfx::Insets::TLBR(16, 4, 0, 4);

// This extra border is added to `CreateImageButton` to make it consistent with
// other buttons in the video conference bubble.
constexpr gfx::Insets kCreateImageButtonBorderInsets = gfx::Insets::VH(8, 0);

constexpr int kCreateImageButtonBetweenChildSpacing = 16;
constexpr int kSetCameraBackgroundViewBetweenChildSpacing = 8;
constexpr int kSetCameraBackgroundViewRadius = 16;
constexpr int kButtonHeight = 20;

// Button for "Create with AI".
class CreateImageButton : public views::LabelButton {
  METADATA_HEADER(CreateImageButton, views::LabelButton)

 public:
  CreateImageButton()
      : views::LabelButton(
            base::BindRepeating(&CreateImageButton::OnButtonClicked,
                                base::Unretained(this)),
            l10n_util::GetStringUTF16(
                IDS_ASH_VIDEO_CONFERENCE_CREAT_WITH_AI_NAME)) {
    SetBorder(views::CreateEmptyBorder(kCreateImageButtonBorderInsets));
    SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
    SetImageLabelSpacing(kCreateImageButtonBetweenChildSpacing);
    SetBackground(views::CreateThemedRoundedRectBackground(
        cros_tokens::kCrosSysSystemOnBase, kSetCameraBackgroundViewRadius));
    SetImageModel(ButtonState::STATE_NORMAL,
                  ui::ImageModel::FromVectorIcon(
                      kAiWandIcon, ui::kColorMenuIcon, kButtonHeight));
  }

  CreateImageButton(const CreateImageButton&) = delete;
  CreateImageButton& operator=(const CreateImageButton&) = delete;
  ~CreateImageButton() override = default;

 private:
  void OnButtonClicked(const ui::Event& event) {}
};

BEGIN_METADATA(CreateImageButton)
END_METADATA

}  // namespace

SetCameraBackgroundView::SetCameraBackgroundView() {
  SetID(BubbleViewID::kSetCameraBackgroundView);

  // `SetCameraBackgroundView` has 2+ children, we want to stack them
  // vertically.
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      /*inside_border_insets=*/kSetCameraBackgroundViewInsideBorderInsets,
      /*between_child_spacing=*/kSetCameraBackgroundViewBetweenChildSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  AddChildView(std::make_unique<CreateImageButton>());
}

BEGIN_METADATA(SetCameraBackgroundView)
END_METADATA

}  // namespace ash::video_conference
