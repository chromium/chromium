
// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/bubble/toggle_effects_view.h"

#include <algorithm>
#include <memory>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/icon_button.h"
#include "ash/system/video_conference/bubble/bubble_view_ids.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "ash/utility/haptics_util.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

constexpr int kButtonCornerRadius = 16;
constexpr int kIconSize = 20;
constexpr int kButtonHeight = 64;
constexpr int kButtonSpacing = 8;

// A single toggle button for a video conference effect, combined with a text
// label. WARNING: `callback` provided must not destroy the button or the bubble
// (i.e. close the bubble) as it would result in a crash in `OnButtonClicked()`.
class ButtonContainer : public views::Button {
 public:
  METADATA_HEADER(ButtonContainer);

  ButtonContainer(views::Button::PressedCallback callback,
                  const gfx::VectorIcon* vector_icon,
                  bool toggle_state,
                  const std::u16string& label_text,
                  const int accessible_name_id,
                  const int preferred_width,
                  absl::optional<int> container_id = absl::nullopt)
      : callback_(callback), toggled_(toggle_state) {
    SetCallback(base::BindRepeating(&ButtonContainer::OnButtonClicked,
                                    weak_ptr_factory_.GetWeakPtr()));
    SetID(video_conference::BubbleViewID::kToggleEffectsButton);

    views::FlexLayout* layout =
        SetLayoutManager(std::make_unique<views::FlexLayout>());
    layout->SetOrientation(views::LayoutOrientation::kVertical);
    layout->SetMainAxisAlignment(views::LayoutAlignment::kCenter);
    layout->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

    // This makes the view the expand or contract to occupy any available space.
    SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                                 views::MaximumFlexSizeRule::kUnbounded));

    // `preferred_width` is assigned by the row this buttons resides in,
    // `kButtonHeight` is from the spec.
    SetPreferredSize(gfx::Size(preferred_width, kButtonHeight));

    auto icon = std::make_unique<views::ImageView>();
    icon->SetImage(ui::ImageModel::FromVectorIcon(
        *vector_icon, cros_tokens::kCrosSysOnSurface, kIconSize));
    icon_ = AddChildView(std::move(icon));

    // Label is below the button.
    AddChildView(std::make_unique<views::Label>(label_text));

    SetTooltipText(l10n_util::GetStringUTF16(accessible_name_id));
    UpdateColorsAndBackground();

    // Assign the ID, if present, to the outermost container view. Only used in
    // tests.
    if (container_id.has_value()) {
      SetID(container_id.value());
    }
  }

  ButtonContainer(const ButtonContainer&) = delete;
  ButtonContainer& operator=(const ButtonContainer&) = delete;

  ~ButtonContainer() override = default;

 private:
  // Callback for clicking the button.
  void OnButtonClicked(const ui::Event& event) {
    callback_.Run(event);

    // Sets the toggled state.
    toggled_ = !toggled_;

    haptics_util::PlayHapticToggleEffect(
        !toggled_, ui::HapticTouchpadEffectStrength::kMedium);

    UpdateColorsAndBackground();
  }

  void UpdateColorsAndBackground() {
    ui::ColorId background_color_id =
        toggled_ ? cros_tokens::kCrosSysSystemPrimaryContainer
                 : cros_tokens::kCrosSysSystemOnBase;

    SetBackground(views::CreateThemedRoundedRectBackground(
        background_color_id, kButtonCornerRadius));
  }

  views::Button::PressedCallback callback_;

  // Indicates the toggled state of the button.
  bool toggled_ = false;

  // Owned by the views hierarchy.
  views::ImageView* icon_ = nullptr;

  base::WeakPtrFactory<ButtonContainer> weak_ptr_factory_{this};
};

BEGIN_METADATA(ButtonContainer, views::Button);
END_METADATA

}  // namespace

namespace video_conference {

ToggleEffectsView::ToggleEffectsView(VideoConferenceTrayController* controller,
                                     const int parent_width) {
  SetID(BubbleViewID::kToggleEffectsView);

  // Layout for the entire toggle effects section.
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetDefault(views::kMarginsKey,
                  gfx::Insets::TLBR(0, 0, kButtonSpacing, 0));

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
        .SetDefault(
            views::kMarginsKey,
            gfx::Insets::TLBR(0, kButtonSpacing / 2, 0, kButtonSpacing / 2));

    // All buttons in a single row should have the same width i.e. fraction of
    // the parent width.
    const int button_width =
        (GetPreferredSize().width() - kButtonSpacing) / row.size();

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
      row_view->AddChildView(std::make_unique<ButtonContainer>(
          state->button_callback(), state->icon(), toggle_state,
          state->label_text(), state->accessible_name_id(), button_width,
          tile->container_id()));
    }

    // Add the row as a child, now that it's fully populated,
    AddChildView(std::move(row_view));
  }
}

BEGIN_METADATA(ToggleEffectsView, views::View);
END_METADATA

}  // namespace video_conference

}  // namespace ash
