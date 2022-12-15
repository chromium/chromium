// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/effects/fake_video_conference_effects.h"

#include <string>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "base/functional/bind.h"
#include "ui/views/controls/button/button.h"

namespace ash::fake_video_conference {

SimpleToggleEffect::SimpleToggleEffect(const std::u16string& label_text)
    : SimpleToggleEffect(/*label_text=*/label_text,
                         /*icon=*/absl::nullopt,
                         /*accessible_name_id=*/absl::nullopt) {}

SimpleToggleEffect::SimpleToggleEffect(
    const std::u16string& label_text,
    absl::optional<const gfx::VectorIcon*> icon,
    absl::optional<int> accessible_name_id) {
  std::unique_ptr<VcHostedEffect> effect =
      std::make_unique<VcHostedEffect>(VcEffectType::kToggle);

  // Use default `icon` and/or `accessible_name_id` if none was passed in.
  std::unique_ptr<VcEffectState> state = std::make_unique<VcEffectState>(
      icon.value_or(&ash::kPrivacyIndicatorsCameraIcon), label_text,
      accessible_name_id.value_or(IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA),
      base::BindRepeating(&SimpleToggleEffect::OnEffectControlActivated,
                          base::Unretained(this),
                          /*effect_id=*/VcEffectState::kUnusedId,
                          /*value=*/0));
  effect->AddState(std::move(state));
  AddEffect(std::move(effect));
}

int SimpleToggleEffect::GetEffectState(int effect_id) {
  return VcHostedEffect::kOff;
}

void SimpleToggleEffect::OnEffectControlActivated(int effect_id, int value) {
  ++num_activations_for_testing_;
}

CatEarsEffect::CatEarsEffect()
    : SimpleToggleEffect(
          /*label_text=*/u"Cat Ears") {}

DogFurEffect::DogFurEffect()
    : SimpleToggleEffect(
          /*label_text=*/u"Dog Fur") {}

SpaceshipEffect::SpaceshipEffect()
    : SimpleToggleEffect(
          /*label_text=*/u"Spaceship") {}

OfficeBunnyEffect::OfficeBunnyEffect()
    : SimpleToggleEffect(
          /*label_text=*/u"Office Bunny") {}

CalmForestEffect::CalmForestEffect()
    : SimpleToggleEffect(
          /*label_text=*/u"Calm Forest") {}

StylishKitchenEffect::StylishKitchenEffect()
    : SimpleToggleEffect(
          /*label_text=*/u"Stylish Kitchen") {}

GreenhouseEffect::GreenhouseEffect()
    : SimpleToggleEffect(
          /*label_text=*/u"Greenhouse") {}

// Delegates that host a set-value effect.

ShaggyFurEffect::ShaggyFurEffect() {
  std::unique_ptr<VcHostedEffect> effect =
      std::make_unique<VcHostedEffect>(VcEffectType::kSetValue);
  std::unique_ptr<VcEffectState> bald_state = std::make_unique<VcEffectState>(
      /*icon=*/nullptr,
      /*label_text=*/u"Bald",
      /*accessible_name_id=*/IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA,
      /*button_callback=*/
      base::BindRepeating(&ShaggyFurEffect::OnEffectControlActivated,
                          base::Unretained(this),
                          /*effect_id=*/0,
                          /*value=*/static_cast<int>(FurShagginess::kBald)));
  std::unique_ptr<VcEffectState> buzzcut_state =
      std::make_unique<VcEffectState>(
          /*icon=*/nullptr,
          /*label_text=*/u"Buzzcut",
          /*accessible_name_id=*/IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA,
          /*button_callback=*/
          base::BindRepeating(
              &ShaggyFurEffect::OnEffectControlActivated,
              base::Unretained(this),
              /*effect_id=*/0,
              /*value=*/static_cast<int>(FurShagginess::kBuzzcut)));
  std::unique_ptr<VcEffectState> thick_state = std::make_unique<VcEffectState>(
      /*icon=*/nullptr,
      /*label_text=*/u"Thick",
      /*accessible_name_id=*/IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA,
      /*button_callback=*/
      base::BindRepeating(&ShaggyFurEffect::OnEffectControlActivated,
                          base::Unretained(this),
                          /*effect_id=*/0,
                          /*value=*/static_cast<int>(FurShagginess::kThick)));
  effect->AddState(std::move(bald_state));
  effect->AddState(std::move(buzzcut_state));
  effect->AddState(std::move(thick_state));
  effect->set_label_text(u"Shaggy Fur");
  effect->set_id(100);
  AddEffect(std::move(effect));

  // Initialize click counts.
  for (int i = 0; i < static_cast<int>(FurShagginess::kMaxNumValues); ++i) {
    num_activations_for_testing_.push_back(0);
  }
}

ShaggyFurEffect::~ShaggyFurEffect() = default;

int ShaggyFurEffect::GetEffectState(int effect_id) {
  return static_cast<int>(FurShagginess::kBuzzcut);
}

void ShaggyFurEffect::OnEffectControlActivated(int effect_id, int value) {
  DCHECK(value >= 0 && value < static_cast<int>(FurShagginess::kMaxNumValues));
  ++num_activations_for_testing_[value];
}

int ShaggyFurEffect::GetNumActivationsForTesting(int value) {
  DCHECK(value >= 0 && value < static_cast<int>(FurShagginess::kMaxNumValues));
  return num_activations_for_testing_[value];
}

SuperCutnessEffect::SuperCutnessEffect() {
  std::unique_ptr<VcHostedEffect> effect =
      std::make_unique<VcHostedEffect>(VcEffectType::kSetValue);
  std::unique_ptr<VcEffectState> ugly_dog_state =
      std::make_unique<VcEffectState>(
          /*icon=*/nullptr,
          /*label_text=*/u"Ugly Dog",
          /*accessible_name_id=*/IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA,
          /*button_callback=*/
          base::BindRepeating(&SuperCutnessEffect::OnEffectControlActivated,
                              base::Unretained(this),
                              /*effect_id=*/0,
                              /*value=*/static_cast<int>(HowCute::kUglyDog)));
  std::unique_ptr<VcEffectState> teddy_bear_state =
      std::make_unique<VcEffectState>(
          /*icon=*/nullptr,
          /*label_text=*/u"Teddy Bear",
          /*accessible_name_id=*/IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA,
          /*button_callback=*/
          base::BindRepeating(&SuperCutnessEffect::OnEffectControlActivated,
                              base::Unretained(this),
                              /*effect_id=*/0,
                              /*value=*/static_cast<int>(HowCute::kTeddyBear)));
  std::unique_ptr<VcEffectState> zara_state = std::make_unique<VcEffectState>(
      /*icon=*/nullptr,
      /*label_text=*/u"Zara",
      /*accessible_name_id=*/IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA,
      /*button_callback=*/
      base::BindRepeating(&SuperCutnessEffect::OnEffectControlActivated,
                          base::Unretained(this),
                          /*effect_id=*/0,
                          /*value=*/static_cast<int>(HowCute::kZara)));
  std::unique_ptr<VcEffectState> inscrutable_state =
      std::make_unique<VcEffectState>(
          /*icon=*/nullptr,
          /*label_text=*/u"Inscrutable",
          /*accessible_name_id=*/IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA,
          /*button_callback=*/
          base::BindRepeating(
              &SuperCutnessEffect::OnEffectControlActivated,
              base::Unretained(this),
              /*effect_id=*/0,
              /*value=*/static_cast<int>(HowCute::kInscrutable)));
  effect->AddState(std::move(ugly_dog_state));
  effect->AddState(std::move(teddy_bear_state));
  effect->AddState(std::move(zara_state));
  effect->AddState(std::move(inscrutable_state));
  effect->set_label_text(u"Super Cuteness");
  effect->set_id(200);
  AddEffect(std::move(effect));

  // Initialize click counts.
  for (int i = 0; i < static_cast<int>(HowCute::kMaxNumValues); ++i) {
    num_activations_for_testing_.push_back(0);
  }
}

SuperCutnessEffect::~SuperCutnessEffect() = default;

int SuperCutnessEffect::GetEffectState(int effect_id) {
  return static_cast<int>(HowCute::kTeddyBear);
}

void SuperCutnessEffect::OnEffectControlActivated(int effect_id, int value) {
  DCHECK(value >= 0 && value < static_cast<int>(HowCute::kMaxNumValues));
  ++num_activations_for_testing_[value];
}

int SuperCutnessEffect::GetNumActivationsForTesting(int value) {
  DCHECK(value >= 0 && value < static_cast<int>(HowCute::kMaxNumValues));
  return num_activations_for_testing_[value];
}

}  // namespace ash::fake_video_conference
