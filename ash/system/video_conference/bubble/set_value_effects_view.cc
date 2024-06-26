// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/bubble/set_value_effects_view.h"

#include "ash/bubble/bubble_utils.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/tab_slider.h"
#include "ash/style/tab_slider_button.h"
#include "ash/style/typography.h"
#include "ash/system/camera/camera_effects_controller.h"
#include "ash/system/video_conference/bubble/bubble_view_ids.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_delegate.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "ash/system/video_conference/resources/grit/vc_resources.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "ash/system/video_conference/video_conference_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/animated_image_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view.h"

namespace ash::video_conference {

namespace {
constexpr int kIconSize = 20;
constexpr float kVcDisabledButtonOpacity = 0.38f;

// Returns a gradient lottie animation defined in the resource file for the
// `Image` button.
std::unique_ptr<lottie::Animation> GetGradientAnimation(
    const ui::ColorProvider* color_provider) {
  std::optional<std::vector<uint8_t>> lottie_data =
      ui::ResourceBundle::GetSharedInstance().GetLottieData(
          IDR_VC_IMAGE_BUTTON_ANIMATION);
  CHECK(lottie_data.has_value());

  return std::make_unique<lottie::Animation>(
      cc::SkottieWrapper::UnsafeCreateSerializable(lottie_data.value()),
      video_conference_utils::CreateColorMapForGradientAnimation(
          color_provider));
}

// Button for "Create with AI".
class AnimatedImageButton : public TabSliderButton {
  METADATA_HEADER(AnimatedImageButton, views::Button)

 public:
  explicit AnimatedImageButton(VideoConferenceTrayController* controller,
                               const VcHostedEffect* effect,
                               const VcEffectState* state)
      : TabSliderButton(
            base::BindRepeating(&AnimatedImageButton::OnButtonClicked,
                                base::Unretained(this)),
            state->label_text()),
        controller_(controller),
        effect_(effect),
        state_(state) {
    // TODO(b/334205690): Use view builder pattern.
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical,
        /*inside_border_insets=*/gfx::Insets(8),
        /*between_child_spacing=*/6));

    auto* animated_view_container =
        AddChildView(std::make_unique<views::View>());
    animated_view_container->SetLayoutManager(
        std::make_unique<views::FillLayout>());

    lottie_animation_view_ = animated_view_container->AddChildView(
        std::make_unique<views::AnimatedImageView>());

    auto* image_view_container =
        animated_view_container->AddChildView(std::make_unique<views::View>());
    image_view_container->SetLayoutManager(
        std::make_unique<views::FillLayout>());
    auto* image_view = image_view_container->AddChildView(
        std::make_unique<views::ImageView>());
    image_view->SetImage(ui::ImageModel::FromImageGenerator(
        base::BindRepeating(
            [](TabSliderButton* tab_slider_button,
               const gfx::VectorIcon* vector_icon, const ui::ColorProvider*) {
              return gfx::CreateVectorIcon(
                  *vector_icon, kIconSize,
                  tab_slider_button->GetColorProvider()->GetColor(
                      tab_slider_button->GetColorIdOnButtonState()));
            },
            /*tab_slider_button=*/this, state->icon()),
        gfx::Size(kIconSize, kIconSize)));

    label_ = AddChildView(std::make_unique<views::Label>(state->label_text()));
    label_->SetAutoColorReadabilityEnabled(false);
    TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton2,
                                          *label_);
  }

  AnimatedImageButton(const AnimatedImageButton&) = delete;
  AnimatedImageButton& operator=(const AnimatedImageButton&) = delete;
  ~AnimatedImageButton() override = default;

  // Reset the animated image on theme changed to get correct color for the
  // animation if the `lottie_animation_view_` should be shown and is visible.
  void OnThemeChanged() override {
    TabSliderButton::OnThemeChanged();
    if (!controller_->ShouldShowImageButtonAnimation() ||
        !lottie_animation_view_->GetVisible()) {
      return;
    }

    lottie_animation_view_->SetAnimatedImage(
        GetGradientAnimation(GetColorProvider()));
    lottie_animation_view_->Play();
  }

  // We should only play the animation when animation view should be shown.
  void AddedToWidget() override {
    if (!controller_->ShouldShowImageButtonAnimation()) {
      lottie_animation_view_->SetVisible(false);
      return;
    }

    if (!lottie_animation_view_->animated_image()) {
      lottie_animation_view_->SetAnimatedImage(
          GetGradientAnimation(GetColorProvider()));
    }
    lottie_animation_view_->Play();
    stop_animation_timer_.Start(FROM_HERE, kGradientAnimationDuration, this,
                                &AnimatedImageButton::HideAnimationView);
  }

  void OnButtonClicked(const ui::Event& event) {
    HideAnimationView();

    if (effect_->delegate()) {
      effect_->delegate()->RecordMetricsForSetValueEffectOnClick(
          effect_->id(), state_->state_value().value());
    }
    state_->button_callback().Run();
    controller_->DismissImageButtonAnimationForever();
  }

  // Update label and image color on selected state changed.
  void OnSelectedChanged() override {
    label_->SetEnabledColorId(GetColorIdOnButtonState());
    // `SchedulePaint()` will result in the `gfx::VectorIcon` for `image_view_`
    // getting re-generated with the proper color.
    SchedulePaint();
  }

  void HideAnimationView() {
    if (!lottie_animation_view_->GetVisible()) {
      return;
    }
    stop_animation_timer_.Stop();
    lottie_animation_view_->Stop();
    lottie_animation_view_->SetVisible(false);
  }

 private:
  raw_ptr<VideoConferenceTrayController> controller_;

  // Information about the associated video conferencing effect needed to
  // display the UI of the tile controlled by this controller.
  const raw_ptr<const VcHostedEffect> effect_;
  const raw_ptr<const VcEffectState> state_;

  // Owned by the View's hierarchy. Used to play the animation on the image.
  raw_ptr<views::AnimatedImageView> lottie_animation_view_ = nullptr;
  // Owned by the View's hierarchy. It's the text shown on `this`.
  raw_ptr<views::Label> label_ = nullptr;

  // Started when `lottie_animation_view_` starts playing the animation. It's
  // used to stop the animation after the animation duration.
  base::OneShotTimer stop_animation_timer_;
};

BEGIN_METADATA(AnimatedImageButton)
END_METADATA

}  // namespace

SetValueEffectSlider::SetValueEffectSlider(
    VideoConferenceTrayController* controller,
    const VcHostedEffect* effect)
    : effect_id_(effect->id()) {
  SetID(BubbleViewID::kSingleSetValueEffectView);
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      /*inside_border_insets=*/gfx::Insets(),
      /*between_child_spacing=*/16));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  if (!effect->label_text().empty()) {
    auto* label_container =
        AddChildView(std::make_unique<views::BoxLayoutView>());
    label_container->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
    label_container->SetMainAxisAlignment(
        views::BoxLayout::MainAxisAlignment::kStart);
    label_container->SetInsideBorderInsets(gfx::Insets::TLBR(0, 4, 0, 0));

    auto* label = label_container->AddChildView(
        std::make_unique<views::Label>(effect->label_text()));
    label->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
    label->SetAutoColorReadabilityEnabled(false);
    TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton2,
                                          *label);
    label->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);

    auto* spacer_view =
        label_container->AddChildView(std::make_unique<views::View>());
    // Let the spacer fill the remaining space, pushing the label to the
    // start.
    label_container->SetFlexForView(spacer_view, 1);
  }

  // If a container ID has been provided then assign it, otherwise assign the
  // default ID.
  SetID(
      effect->container_id().value_or(BubbleViewID::kSingleSetValueEffectView));

  // `effect` is expected to provide the current state of the effect, and
  // a `current_state` with no value means it couldn't be obtained.
  std::optional<int> current_state = effect->get_state_callback().Run();
  DCHECK(current_state.has_value());

  const int num_states = effect->GetNumStates();
  const int max_num_states =
      ::ash::features::IsVcBackgroundReplaceEnabled() ? 4 : 3;
  DCHECK_LE(num_states, max_num_states)
      << "UX Requests no more than " << max_num_states
      << " states, otherwise "
         "the bubble will need to be wider.";

  auto tab_slider = std::make_unique<TabSlider>(
      num_states, IconLabelSliderButton::kSliderParams);
  for (int i = 0; i < num_states; ++i) {
    const VcEffectState* state = effect->GetState(/*index=*/i);
    DCHECK(state->state_value());

    const bool is_image_button =
        state->view_id() ==
        video_conference::BubbleViewID::kBackgroundBlurImageButton;

    TabSliderButton* slider_button;
    if (is_image_button && !state->is_disabled_by_enterprise()) {
      slider_button = tab_slider->AddButton(
          std::make_unique<AnimatedImageButton>(controller, effect, state));
    } else {
      slider_button =
          tab_slider->AddButton(std::make_unique<IconLabelSliderButton>(
              base::BindRepeating(
                  [](const VcHostedEffect* effect, const VcEffectState* state) {
                    if (effect->delegate()) {
                      effect->delegate()->RecordMetricsForSetValueEffectOnClick(
                          effect->id(), state->state_value().value());
                    }

                    state->button_callback().Run();
                  },
                  base::Unretained(effect), base::Unretained(state)),
              state->icon(), state->label_text()));
    }

    if (state->is_disabled_by_enterprise()) {
      // Disable button.
      slider_button->SetState(views::Button::ButtonState::STATE_DISABLED);
      // Add tooltip to indicate why it is disabled.
      slider_button->SetTooltipText(l10n_util::GetStringUTF16(
          IDS_ASH_VIDEO_CONFERENCE_BUBBLE_BACKGROUND_DISABLED_TOOLTIP));
      // Set opacity.
      slider_button->layer()->SetOpacity(kVcDisabledButtonOpacity);
      // Set accessibility name.
      slider_button->GetViewAccessibility().SetName(state->label_text());
    }

    slider_button->SetSelected(state->state_value().value() == current_state);

    if (state->view_id() != -1) {
      slider_button->SetID(state->view_id());
    }
  }
  tab_slider_ = AddChildView(std::move(tab_slider));
}

BEGIN_METADATA(SetValueEffectSlider)
END_METADATA

SetValueEffectsView::SetValueEffectsView(
    VideoConferenceTrayController* controller) {
  SetID(BubbleViewID::kSetValueEffectsView);

  views::FlexLayout* layout =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical);
  layout->SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  layout->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);

  if (controller->GetEffectsManager().HasSetValueEffects()) {
    for (auto* effect : controller->GetEffectsManager().GetSetValueEffects()) {
      // If the current state of `effect` has no value, it means the state of
      // the effect cannot be obtained. This can happen if the
      // `VcEffectsDelegate` hosting `effect` has encountered an error or is
      // in some bad state. In this case its controls are not presented.
      if (!effect->get_state_callback().Run().has_value()) {
        continue;
      }

      AddChildView(std::make_unique<SetValueEffectSlider>(controller, effect));
    }
  }
}

BEGIN_METADATA(SetValueEffectsView)
END_METADATA

}  // namespace ash::video_conference
