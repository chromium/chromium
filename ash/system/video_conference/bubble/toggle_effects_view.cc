
// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/bubble/toggle_effects_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/bubble/bubble_utils.h"
#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/style/typography.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/video_conference/bubble/bubble_view_ids.h"
#include "ash/system/video_conference/bubble/vc_tile_ui_controller.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "ash/system/video_conference/video_conference_utils.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/utils/haptics_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/events/event.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
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
  METADATA_HEADER(ToggleEffectsButtonLabel, views::Label)

 public:
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

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds &available_size) const override {
    // TODO(crbug.com/40233803): The size constraint is not passed down from
    // the views tree in the first round of layout, so multiline label might
    // be broken here. We need to explicitly set the size to fix this.
    return gfx::Size(label_max_width_,
                     views::Label::CalculatePreferredSize(
                         views::SizeBounds(label_max_width_, {}))
                         .height());
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

BEGIN_METADATA(ToggleEffectsButtonLabel);
END_METADATA

}  // namespace

ToggleEffectsButton::ToggleEffectsButton(
    views::Button::PressedCallback callback,
    const gfx::VectorIcon* vector_icon,
    bool toggle_state,
    const std::u16string& label_text,
    const int accessible_name_id,
    std::optional<int> container_id,
    const VcEffectId effect_id,
    int num_button_per_row)
    : callback_(std::move(callback)),
      toggled_(toggle_state),
      effect_id_(effect_id),
      vector_icon_(vector_icon),
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

  // If VcDlcUi is enabled then the button's preferred size and flex properties
  // are controlled externally to the button rather than by the button itself.
  if (!features::IsVcDlcUiEnabled()) {
    // This makes the view the expand or contract to occupy any available space.
    SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                                 views::MaximumFlexSizeRule::kUnbounded));

    SetPreferredSize(gfx::Size(GetPreferredSize().width(), kButtonHeight));
  }

  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                kButtonCornerRadius);

  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetColorId(cros_tokens::kCrosSysFocusRing);
  // The focus ring appears slightly outside the tile bounds.
  focus_ring->SetHaloInset(-3);
  // Since the focus ring doesn't set a LayoutManager it won't get drawn
  // unless excluded by the tile's LayoutManager.
  focus_ring->SetProperty(views::kViewIgnoredByLayoutKey, true);

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
  GetViewAccessibility().SetRole(ax::mojom::Role::kToggleButton);
  SetFocusBehavior(FocusBehavior::ALWAYS);

  UpdateColorsAndBackground();

  // Assign the ID, if present, to the outermost container view. Only used in
  // tests.
  if (container_id.has_value()) {
    SetID(container_id.value());
  }

  VideoConferenceTrayController::Get()->GetEffectsManager().AddObserver(this);
}

ToggleEffectsButton::~ToggleEffectsButton() {
  VideoConferenceTrayController::Get()->GetEffectsManager().RemoveObserver(
      this);
}

void ToggleEffectsButton::OnEffectChanged(VcEffectId effect_id, bool is_on) {
  if (effect_id != effect_id_ || is_on == toggled_) {
    return;
  }

  toggled_ = is_on;
  UpdateColorsAndBackground();
  UpdateTooltip();
}

void ToggleEffectsButton::OnButtonClicked(const ui::Event& event) {
  // Sets the toggled state.
  toggled_ = !toggled_;

  // Run `callback_` after `toggled_` is updated to avoid duplicated work with
  // OnCameraEffectChange().
  callback_.Run(event);

  base::UmaHistogramBoolean(
      video_conference_utils::GetEffectHistogramNameForClick(effect_id_),
      toggled_);

  chromeos::haptics_util::PlayHapticToggleEffect(
      !toggled_, ui::HapticTouchpadEffectStrength::kMedium);

  UpdateColorsAndBackground();
  UpdateTooltip();
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
      *vector_icon_, foreground_color_id, kIconSize));
  label_->SetEnabledColorId(foreground_color_id);
}

void ToggleEffectsButton::UpdateTooltip() {
  SetTooltipText(l10n_util::GetStringFUTF16(
      VIDEO_CONFERENCE_TOGGLE_BUTTON_TOOLTIP,
      l10n_util::GetStringUTF16(accessible_name_id_),
      l10n_util::GetStringUTF16(
          toggled_ ? VIDEO_CONFERENCE_TOGGLE_BUTTON_STATE_ON
                   : VIDEO_CONFERENCE_TOGGLE_BUTTON_STATE_OFF)));
}

BEGIN_METADATA(ToggleEffectsButton);
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
                  gfx::Insets::TLBR(0, 0, kButtonContainerSpacing, 0))
      .SetIgnoreDefaultMainAxisMargins(true);

  // The effects manager provides the toggle effects in rows.
  auto& effects_manager = controller->GetEffectsManager();
  const VideoConferenceTrayEffectsManager::EffectDataTable tile_rows =
      effects_manager.GetToggleEffectButtonTable();
  for (auto& row : tile_rows) {
    // Each row is its own view, with its own layout.
    std::unique_ptr<views::View> row_view = std::make_unique<views::View>();
    row_view->SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kHorizontal)
        .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
        .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
        .SetDefault(views::kMarginsKey,
                    gfx::Insets::TLBR(0, kButtonContainerSpacing / 2, 0,
                                      kButtonContainerSpacing / 2))
        .SetIgnoreDefaultMainAxisMargins(true);

    // TODO(crbug.com/40232718): See View::SetLayoutManagerUseConstrainedSpace.
    row_view->SetLayoutManagerUseConstrainedSpace(false);

    // Add a button for each item in the row.
    for (auto* tile : row) {
      DCHECK_EQ(tile->type(), VcEffectType::kToggle);
      DCHECK_EQ(tile->GetNumStates(), 1);

      // If `current_state` has no value, it means the state of the effect
      // (represented by `tile`) cannot be obtained. This can happen if the
      // `VcEffectsDelegate` hosting the effect has encountered an error or is
      // in some bad state. In this case its controls are not presented.
      std::optional<int> current_state = tile->get_state_callback().Run();
      if (!current_state.has_value()) {
        continue;
      }

      // `current_state` can only be a `bool` for a toggle effect.
      bool toggle_state = current_state.value() != 0;
      const VcEffectState* state = tile->GetState(/*index=*/0);

      // The button should either be a `FeatureTile` or a `ToggleEffectsButton`,
      // depending on the following logic.
      std::unique_ptr<views::View> button;
      if (ash::features::IsVcDlcUiEnabled()) {
        // If VcDlcUi is enabled then first try to see if the button should be a
        // `FeatureTile` by determining if there is a tile controller for the
        // VC effect.
        auto* tile_controller =
            effects_manager.GetUiControllerForEffectId(tile->id());
        if (tile_controller) {
          button = tile_controller->CreateTile();
        }
      }
      if (!button) {
        // If there was no tile controller or if VcDlcUi is not enabled, then
        // the button should be a `ToggleEffectsButton`.
        button = std::make_unique<ToggleEffectsButton>(
            state->button_callback(), state->icon(), toggle_state,
            state->label_text(), state->accessible_name_id(),
            tile->container_id(), tile->id(),
            /*num_button_per_row=*/row.size());
      }

      // If VcDlcUi is enabled then the button's preferred size and flex
      // properties are controlled externally to the button rather than by the
      // button itself.
      if (ash::features::IsVcDlcUiEnabled()) {
        // Set the preferred width to 0 so that the container (`row_view`) can
        // equally distribute its full width among all its child buttons.
        button->SetPreferredSize(gfx::Size(0, kButtonHeight));

        // Allow the button to expand or contract to whatever size is available
        // for it.
        button->SetProperty(views::kFlexBehaviorKey,
                            views::FlexSpecification(
                                views::MinimumFlexSizeRule::kScaleToMinimum,
                                views::MaximumFlexSizeRule::kUnbounded));
      }

      row_view->AddChildView(std::move(button));
    }

    // Add the row as a child, now that it's fully populated,
    AddChildView(std::move(row_view));
  }
}

BEGIN_METADATA(ToggleEffectsView);
END_METADATA

}  // namespace ash::video_conference
