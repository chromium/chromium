
// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/bubble/toggle_effects_view.h"

#include <algorithm>
#include <memory>

#include "ash/bubble/bubble_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/style/typography.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/video_conference/bubble/bubble_view_ids.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "ash/system/video_conference/video_conference_utils.h"
#include "ash/utility/haptics_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/events/event.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"

namespace ash::video_conference {

namespace {

constexpr int kButtonCornerRadius = 16;
constexpr int kIconSize = 20;
constexpr int kButtonHeight = 64;
constexpr int kButtonContainerSpacing = 8;
constexpr int kButtonVerticalPadding = 4;
constexpr int kButtonHorizontalPadding = 8;
constexpr int kButtonHorizontalPaddingWithMultilineLabel = 16;
constexpr int kButtonSpacing = 8;
constexpr int kButtonSpacingWithMultilineLabel = 4;
constexpr int kMaxLinesForLabel = 2;

constexpr char kGoogleSansFont[] = "Google Sans";

// A customized label for the toggle effects button. When the label has more
// than 1 line, it will automatically adjust the padding and the spacing of the
// button.
class ToggleEffectsButtonLabel : public views::Label {
 public:
  METADATA_HEADER(ToggleEffectsButtonLabel);

  ToggleEffectsButtonLabel(ToggleEffectsButton* button,
                           const std::u16string& label_text,
                           int num_button_per_row)
      : button_(button), num_button_per_row_(num_button_per_row) {
    // Need to set up `label_max_width_` so that the first round of layout is
    // set up correctly for multiline label (crbug.com/1349528). For this first
    // layout, we will assume that the text is multi line to fix the mentioned
    // bug. A one-line text will be adjusted correctly in subsequent layout(s).
    SetLabelMaxWidth(/*is_multi_line=*/true);

    SetID(video_conference::BubbleViewID::kToggleEffectLabel);
    SetAutoColorReadabilityEnabled(false);
    SetEnabledColorId(cros_tokens::kCrosSysOnPrimaryContainer);
    SetMultiLine(true);
    SetMaxLines(kMaxLinesForLabel);
    SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                 views::MaximumFlexSizeRule::kPreferred));

    // TODO(b/290374705): Use token style when it is available.
    SetFontList(gfx::FontList({kGoogleSansFont}, gfx::Font::NORMAL, 12,
                              gfx::Font::Weight::MEDIUM));
    SetLineHeight(16);

    SetText(label_text);
  }

  ToggleEffectsButtonLabel(const ToggleEffectsButtonLabel&) = delete;
  ToggleEffectsButtonLabel& operator=(const ToggleEffectsButtonLabel&) = delete;

  ~ToggleEffectsButtonLabel() override = default;

  void SetText(const std::u16string& new_text) override {
    views::Label::SetText(new_text);

    // Need to size to the new preferred size to know the number of lines
    // required to display the text. If we display the text in 2 lines, we need
    // to adjust the button horizontal padding and spacing between the icon and
    // the label.
    SizeToPreferredSize();
    bool is_multi_line = GetRequiredLines() > 1;

    SetLabelMaxWidth(is_multi_line);
    SetMaximumWidth(label_max_width_);

    button_->layout()->SetInteriorMargin(gfx::Insets::VH(
        kButtonVerticalPadding, is_multi_line
                                    ? kButtonHorizontalPaddingWithMultilineLabel
                                    : kButtonHorizontalPadding));

    button_->icon()->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
        0, 0, is_multi_line ? kButtonSpacingWithMultilineLabel : kButtonSpacing,
        0)));
  }

  gfx::Size CalculatePreferredSize() const override {
    // TODO(crbug.com/1349528): The size constraint is not passed down from
    // the views tree in the first round of layout, so multiline label might
    // be broken here. We need to explicitly set the size to fix this.
    return gfx::Size(label_max_width_, GetHeightForWidth(label_max_width_));
  }

 private:
  // Set `label_max_width_` based on whether the label has more than 1 line.
  void SetLabelMaxWidth(bool is_multi_line) {
    int button_width =
        (kTrayMenuWidth - kVideoConferenceBubbleHorizontalPadding * 2 -
         kButtonContainerSpacing -
         kButtonContainerSpacing * (num_button_per_row_ - 1)) /
        num_button_per_row_;

    auto horizontal_padding = is_multi_line
                                  ? kButtonHorizontalPaddingWithMultilineLabel
                                  : kButtonHorizontalPadding;

    label_max_width_ = button_width - horizontal_padding * 2;
  }

  raw_ptr<ToggleEffectsButton> button_ = nullptr;

  // Keeps track of the number of buttons that is in the row that this label
  // resides in. Used to calculate the max width of this label.
  const int num_button_per_row_;

  int label_max_width_ = 0;
};

BEGIN_METADATA(ToggleEffectsButtonLabel, views::Label);
END_METADATA

}  // namespace

ToggleEffectsButton::ToggleEffectsButton(
    views::Button::PressedCallback callback,
    const gfx::VectorIcon* enabled_vector_icon,
    const gfx::VectorIcon* disabled_vector_icon,
    bool toggle_state,
    const std::u16string& label_text,
    const int accessible_name_id,
    absl::optional<int> container_id,
    const VcEffectId effect_id,
    int num_button_per_row)
    : callback_(callback),
      toggled_(toggle_state),
      effect_id_(effect_id),
      enabled_vector_icon_(enabled_vector_icon),
      disabled_vector_icon_(disabled_vector_icon),
      accessible_name_id_(accessible_name_id) {
  SetCallback(base::BindRepeating(&ToggleEffectsButton::OnButtonClicked,
                                  weak_ptr_factory_.GetWeakPtr()));
  SetID(video_conference::BubbleViewID::kToggleEffectsButton);

  layout_ = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout_->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetInteriorMargin(
          gfx::Insets::TLBR(kButtonVerticalPadding, kButtonHorizontalPadding,
                            kButtonVerticalPadding, kButtonHorizontalPadding));

  // This makes the view the expand or contract to occupy any available space.
  SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kUnbounded));

  SetPreferredSize(gfx::Size(GetPreferredSize().width(), kButtonHeight));

  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                kButtonCornerRadius);

  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetColorId(cros_tokens::kCrosSysFocusRing);
  // The focus ring appears slightly outside the tile bounds.
  focus_ring->SetHaloInset(-3);
  // Since the focus ring doesn't set a LayoutManager it won't get drawn
  // unless excluded by the tile's LayoutManager.
  layout_->SetChildViewIgnoredByLayout(focus_ring, true);

  auto icon = std::make_unique<views::ImageView>();
  icon->SetID(video_conference::BubbleViewID::kToggleEffectIcon);
  // `icon_` image set in `UpdateColorsAndBackground()`.
  icon_ = AddChildView(std::move(icon));

  auto label = std::make_unique<ToggleEffectsButtonLabel>(
      /*button=*/this, label_text, num_button_per_row);
  label_ = AddChildView(std::move(label));

  SetTooltipText(l10n_util::GetStringFUTF16(
      VIDEO_CONFERENCE_TOGGLE_BUTTON_TOOLTIP,
      l10n_util::GetStringUTF16(accessible_name_id_),
      l10n_util::GetStringUTF16(
          toggled_ ? VIDEO_CONFERENCE_TOGGLE_BUTTON_STATE_ON
                   : VIDEO_CONFERENCE_TOGGLE_BUTTON_STATE_OFF)));
  SetAccessibleRole(ax::mojom::Role::kToggleButton);
  SetFocusBehavior(FocusBehavior::ALWAYS);

  UpdateColorsAndBackground();

  // Assign the ID, if present, to the outermost container view. Only used in
  // tests.
  if (container_id.has_value()) {
    SetID(container_id.value());
  }
}

ToggleEffectsButton::~ToggleEffectsButton() = default;

void ToggleEffectsButton::OnButtonClicked(const ui::Event& event) {
  callback_.Run(event);

  // Sets the toggled state.
  toggled_ = !toggled_;

  base::UmaHistogramBoolean(
      video_conference_utils::GetEffectHistogramNameForClick(effect_id_),
      toggled_);

  haptics_util::PlayHapticToggleEffect(
      !toggled_, ui::HapticTouchpadEffectStrength::kMedium);

  UpdateColorsAndBackground();
  SetTooltipText(l10n_util::GetStringFUTF16(
      VIDEO_CONFERENCE_TOGGLE_BUTTON_TOOLTIP,
      l10n_util::GetStringUTF16(accessible_name_id_),
      l10n_util::GetStringUTF16(
          toggled_ ? VIDEO_CONFERENCE_TOGGLE_BUTTON_STATE_ON
                   : VIDEO_CONFERENCE_TOGGLE_BUTTON_STATE_OFF)));
}

void ToggleEffectsButton::UpdateColorsAndBackground() {
  ui::ColorId background_color_id =
      toggled_ ? cros_tokens::kCrosSysSystemPrimaryContainer
               : cros_tokens::kCrosSysSystemOnBase;
  SetBackground(views::CreateThemedRoundedRectBackground(background_color_id,
                                                         kButtonCornerRadius));

  ui::ColorId foreground_color_id =
      toggled_ ? cros_tokens::kCrosSysSystemOnPrimaryContainer
               : cros_tokens::kCrosSysOnSurface;
  icon_->SetImage(ui::ImageModel::FromVectorIcon(
      toggled_ ? *enabled_vector_icon_ : *disabled_vector_icon_,
      foreground_color_id, kIconSize));
  label_->SetEnabledColorId(foreground_color_id);
}

BEGIN_METADATA(ToggleEffectsButton, views::Button);
END_METADATA

ToggleEffectsView::ToggleEffectsView(
    VideoConferenceTrayController* controller) {
  SetID(BubbleViewID::kToggleEffectsView);

  // Layout for the entire toggle effects section.
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetDefault(views::kMarginsKey,
                  gfx::Insets::TLBR(0, 0, kButtonContainerSpacing, 0));

  // The effects manager provides the toggle effects in rows.
  const VideoConferenceTrayEffectsManager::EffectDataTable tile_rows =
      controller->effects_manager().GetToggleEffectButtonTable();
  for (auto& row : tile_rows) {
    // Each row is its own view, with its own layout.
    std::unique_ptr<views::View> row_view = std::make_unique<views::View>();
    row_view->SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kHorizontal)
        .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
        .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
        .SetDefault(views::kMarginsKey,
                    gfx::Insets::TLBR(0, kButtonContainerSpacing / 2, 0,
                                      kButtonContainerSpacing / 2));

    // Add a button for each item in the row.
    for (auto* tile : row) {
      DCHECK_EQ(tile->type(), VcEffectType::kToggle);
      DCHECK_EQ(tile->GetNumStates(), 1);

      // If `current_state` has no value, it means the state of the effect
      // (represented by `tile`) cannot be obtained. This can happen if the
      // `VcEffectsDelegate` hosting the effect has encountered an error or is
      // in some bad state. In this case its controls are not presented.
      absl::optional<int> current_state = tile->get_state_callback().Run();
      if (!current_state.has_value()) {
        continue;
      }

      // `current_state` can only be a `bool` for a toggle effect.
      bool toggle_state = current_state.value() != 0;
      const VcEffectState* state = tile->GetState(/*index=*/0);
      CHECK(state->disabled_icon())
          << "Toggle effects must define a disabled icon.";
      row_view->AddChildView(std::make_unique<ToggleEffectsButton>(
          state->button_callback(), state->icon(), state->disabled_icon(),
          toggle_state, state->label_text(), state->accessible_name_id(),
          tile->container_id(), tile->id(), /*num_button_per_row=*/row.size()));
    }

    // Add the row as a child, now that it's fully populated,
    AddChildView(std::move(row_view));
  }
}

BEGIN_METADATA(ToggleEffectsView, views::View);
END_METADATA

}  // namespace ash::video_conference
