// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/unified_volume_view.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "base/i18n/rtl.h"
#include "base/stl_util.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"

using chromeos::CrasAudioHandler;

namespace ash {

namespace {

// References to the icons that correspond to different volume levels.
const gfx::VectorIcon* const kVolumeLevelIcons[] = {
    &kUnifiedMenuVolumeLowIcon,     // Low volume.
    &kUnifiedMenuVolumeMediumIcon,  // Medium volume.
    &kUnifiedMenuVolumeHighIcon,    // High volume.
    &kUnifiedMenuVolumeHighIcon,    // Full volume.
};

// The maximum index of kVolumeLevelIcons.
constexpr int kVolumeLevels = base::size(kVolumeLevelIcons) - 1;

// Get vector icon reference that corresponds to the given volume level. |level|
// is between 0.0 to 1.0.
const gfx::VectorIcon& GetVolumeIconForLevel(float level) {
  int index = static_cast<int>(std::ceil(level * kVolumeLevels));
  if (index < 0)
    index = 0;
  else if (index > kVolumeLevels)
    index = kVolumeLevels;
  return *kVolumeLevelIcons[index];
}

SkColor GetBackgroundColorOfMoreButton() {
  return AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
}

class MoreButton : public views::Button {
 public:
  explicit MoreButton(PressedCallback callback)
      : views::Button(std::move(callback)) {
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal,
        gfx::Insets((kTrayItemSize -
                     GetDefaultSizeOfVectorIcon(vector_icons::kHeadsetIcon)) /
                    2),
        2));

    if (!features::IsSystemTrayMicGainSettingEnabled()) {
      headset_image_ = AddChildView(std::make_unique<views::ImageView>());
      headset_image_->SetCanProcessEventsWithinSubtree(false);
    }
    more_image_ = AddChildView(std::make_unique<views::ImageView>());
    more_image_->SetCanProcessEventsWithinSubtree(false);
    SetTooltipText(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUDIO));
    TrayPopupUtils::ConfigureTrayPopupButton(this);

    views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                  kTrayItemCornerRadius);
    SetBackground(views::CreateRoundedRectBackground(
        GetBackgroundColorOfMoreButton(), kTrayItemCornerRadius));
  }

  ~MoreButton() override = default;

  std::unique_ptr<views::InkDrop> CreateInkDrop() override {
    return TrayPopupUtils::CreateInkDrop(this);
  }

  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override {
    return TrayPopupUtils::CreateInkDropRipple(
        TrayPopupInkDropStyle::FILL_BOUNDS, this,
        GetInkDropCenterBasedOnLastEvent());
  }

  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override {
    return TrayPopupUtils::CreateInkDropHighlight(this);
  }

  const char* GetClassName() const override { return "MoreButton"; }

  void OnThemeChanged() override {
    views::Button::OnThemeChanged();
    auto* color_provider = AshColorProvider::Get();
    const SkColor icon_color = color_provider->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kIconColorPrimary);
    if (headset_image_) {
      headset_image_->SetImage(
          CreateVectorIcon(vector_icons::kHeadsetIcon, icon_color));
    }
    DCHECK(more_image_);
    auto icon_rotation = base::i18n::IsRTL()
                             ? SkBitmapOperations::ROTATION_270_CW
                             : SkBitmapOperations::ROTATION_90_CW;
    more_image_->SetImage(gfx::ImageSkiaOperations::CreateRotatedImage(
        CreateVectorIcon(kUnifiedMenuExpandIcon, icon_color), icon_rotation));
    focus_ring()->SetColor(color_provider->GetControlsLayerColor(
        AshColorProvider::ControlsLayerType::kFocusRingColor));
    background()->SetNativeControlColor(GetBackgroundColorOfMoreButton());
  }

 private:
  views::ImageView* headset_image_ = nullptr;
  views::ImageView* more_image_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(MoreButton);
};

}  // namespace

UnifiedVolumeView::UnifiedVolumeView(
    UnifiedVolumeSliderController* controller,
    UnifiedVolumeSliderController::Delegate* delegate)
    : UnifiedSliderView(base::BindRepeating(
                            &UnifiedVolumeSliderController::SliderButtonPressed,
                            base::Unretained(controller)),
                        controller,
                        kSystemMenuVolumeHighIcon,
                        IDS_ASH_STATUS_TRAY_VOLUME_SLIDER_LABEL),
      more_button_(new MoreButton(
          base::BindRepeating(&UnifiedVolumeSliderController::Delegate::
                                  OnAudioSettingsButtonClicked,
                              base::Unretained(delegate)))) {
  CrasAudioHandler::Get()->AddAudioObserver(this);
  AddChildView(more_button_);
  Update(false /* by_user */);
}

UnifiedVolumeView::~UnifiedVolumeView() {
  CrasAudioHandler::Get()->RemoveAudioObserver(this);
}

const char* UnifiedVolumeView::GetClassName() const {
  return "UnifiedVolumeView";
}

void UnifiedVolumeView::Update(bool by_user) {
  bool is_muted = CrasAudioHandler::Get()->IsOutputMuted();
  float level = CrasAudioHandler::Get()->GetOutputVolumePercent() / 100.f;

  // To indicate that the volume is muted, set the volume slider to the minimal
  // visual style.
  slider()->SetRenderingStyle(
      is_muted ? views::Slider::RenderingStyle::kMinimalStyle
               : views::Slider::RenderingStyle::kDefaultStyle);

  // The button should be gray when muted and colored otherwise.
  button()->SetToggled(!is_muted);
  button()->SetVectorIcon(is_muted ? kUnifiedMenuVolumeMuteIcon
                                   : GetVolumeIconForLevel(level));
  base::string16 state_tooltip_text = l10n_util::GetStringUTF16(
      is_muted ? IDS_ASH_STATUS_TRAY_VOLUME_STATE_MUTED
               : IDS_ASH_STATUS_TRAY_VOLUME_STATE_ON);
  button()->SetTooltipText(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_VOLUME, state_tooltip_text));

  more_button_->SetVisible(CrasAudioHandler::Get()->has_alternative_input() ||
                           CrasAudioHandler::Get()->has_alternative_output() ||
                           features::IsSystemTrayMicGainSettingEnabled());

  // Slider's value is in finer granularity than audio volume level(0.01),
  // there will be a small discrepancy between slider's value and volume level
  // on audio side. To avoid the jittering in slider UI, use the slider's
  // current value.
  if (std::abs(level - slider()->GetValue()) <
      kAudioSliderIgnoreUpdateThreshold) {
    level = slider()->GetValue();
  }
  // Note: even if the value does not change, we still need to call this
  // function to enable accessibility events (crbug.com/1013251).
  SetSliderValue(level, by_user);
}

void UnifiedVolumeView::OnOutputNodeVolumeChanged(uint64_t node_id,
                                                  int volume) {
  Update(true /* by_user */);
}

void UnifiedVolumeView::OnOutputMuteChanged(bool mute_on) {
  Update(true /* by_user */);
}

void UnifiedVolumeView::OnAudioNodesChanged() {
  Update(true /* by_user */);
}

void UnifiedVolumeView::OnActiveOutputNodeChanged() {
  Update(true /* by_user */);
}

void UnifiedVolumeView::OnActiveInputNodeChanged() {
  Update(true /* by_user */);
}

void UnifiedVolumeView::ChildVisibilityChanged(views::View* child) {
  Layout();
}

}  // namespace ash
