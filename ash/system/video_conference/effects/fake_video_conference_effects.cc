// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/effects/fake_video_conference_effects.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "base/functional/bind.h"
#include "ui/views/controls/button/button.h"

namespace ash::fake_video_conference {

SimpleToggleEffect::SimpleToggleEffect(const std::u16string& label_text)
    : SimpleToggleEffect(/*label_text=*/label_text,
                         /*icon=*/std::nullopt,
                         /*accessible_name_id=*/std::nullopt) {}

SimpleToggleEffect::~SimpleToggleEffect() = default;

SimpleToggleEffect::SimpleToggleEffect(
    const std::u16string& label_text,
    std::optional<const gfx::VectorIcon*> icon,
    std::optional<int> accessible_name_id) {
  std::unique_ptr<VcHostedEffect> effect = std::make_unique<VcHostedEffect>(
      VcEffectType::kToggle,
      base::BindRepeating(&SimpleToggleEffect::GetEffectState,
                          base::Unretained(this),
                          /*effect_id=*/VcEffectId::kTestEffect),
      VcEffectId::kTestEffect);

  // Use default `icon` and/or `accessible_name_id` if none was passed in.
  std::unique_ptr<VcEffectState> state = std::make_unique<VcEffectState>(
      icon.value_or(&ash::kPrivacyIndicatorsCameraIcon), label_text,
      accessible_name_id.value_or(IDS_PRIVACY_INDICATORS_STATUS_CAMERA),
      base::BindRepeating(&SimpleToggleEffect::OnEffectControlActivated,
                          weak_factory_.GetWeakPtr(),
                          /*effect_id=*/VcEffectId::kTestEffect,
                          /*value=*/std::nullopt));
  effect->AddState(std::move(state));
  AddEffect(std::move(effect));
}

std::optional<int> SimpleToggleEffect::GetEffectState(VcEffectId effect_id) {
  // Subclass `SimpleToggleEffect` if a specific integer or enum value (other
  // than 0) needs to be returned. Returning `std::nullopt` is taken as "no
  // value could be obtained" and treated as an error condition.
  return 0;
}

void SimpleToggleEffect::OnEffectControlActivated(VcEffectId effect_id,
                                                  std::optional<int> state) {
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

FakeLongTextLabelToggleEffect::FakeLongTextLabelToggleEffect()
    : SimpleToggleEffect(
          /*label_text=*/u"Fake Long Text Label Toggle Effect") {}

// Delegates that host a set-value effect.

ShaggyFurEffect::ShaggyFurEffect() {
  std::unique_ptr<VcHostedEffect> effect = std::make_unique<VcHostedEffect>(
      VcEffectType::kSetValue,
      base::BindRepeating(&ShaggyFurEffect::GetEffectState,
                          base::Unretained(this),
                          /*effect_id=*/VcEffectId::kTestEffect),
      VcEffectId::kTestEffect);
  effect->set_label_text(u"Shaggy Fur");
  AddStateToEffect(effect.get(),
                   /*state_value=*/static_cast<int>(FurShagginess::kBald),
                   /*label_text=*/u"Bald");
  AddStateToEffect(effect.get(),
                   /*state_value=*/static_cast<int>(FurShagginess::kBuzzcut),
                   /*label_text=*/u"Buzzcut");
  AddStateToEffect(effect.get(),
                   /*state_value=*/static_cast<int>(FurShagginess::kThick),
                   /*label_text=*/u"Thick");
  AddEffect(std::move(effect));

  // Initialize click counts.
  for (int i = 0; i < static_cast<int>(FurShagginess::kMaxNumValues); ++i) {
    num_activations_for_testing_.push_back(0);
  }
}

ShaggyFurEffect::~ShaggyFurEffect() = default;

std::optional<int> ShaggyFurEffect::GetEffectState(VcEffectId effect_id) {
  return static_cast<int>(FurShagginess::kBuzzcut);
}

void ShaggyFurEffect::OnEffectControlActivated(VcEffectId effect_id,
                                               std::optional<int> state) {
  DCHECK(state.has_value());
  DCHECK(state.value() >= 0 &&
         state.value() < static_cast<int>(FurShagginess::kMaxNumValues));
  ++num_activations_for_testing_[state.value()];
}

int ShaggyFurEffect::GetNumActivationsForTesting(int state_value) {
  CHECK(state_value >= 0 &&
        state_value < static_cast<int>(FurShagginess::kMaxNumValues));
  return num_activations_for_testing_[state_value];
}

void ShaggyFurEffect::AddStateToEffect(VcHostedEffect* effect,
                                       int state_value,
                                       std::u16string label_text) {
  DCHECK(effect);
  effect->AddState(std::make_unique<VcEffectState>(
      /*icon=*/&ash::kPrivacyIndicatorsCameraIcon,
      /*label_text=*/label_text,
      /*accessible_name_id=*/IDS_PRIVACY_INDICATORS_STATUS_CAMERA,
      /*button_callback=*/
      base::BindRepeating(&ShaggyFurEffect::OnEffectControlActivated,
                          weak_factory_.GetWeakPtr(),
                          /*effect_id=*/VcEffectId::kTestEffect,
                          /*value=*/state_value),
      /*state=*/state_value));
}

SuperCutnessEffect::SuperCutnessEffect() {
  std::unique_ptr<VcHostedEffect> effect = std::make_unique<VcHostedEffect>(
      VcEffectType::kSetValue,
      base::BindRepeating(&SuperCutnessEffect::GetEffectState,
                          base::Unretained(this),
                          /*effect_id=*/VcEffectId::kTestEffect),
      VcEffectId::kTestEffect);
  effect->set_label_text(u"Super Cuteness");
  AddStateToEffect(effect.get(),
                   /*state_value=*/static_cast<int>(HowCute::kUglyDog),
                   /*label_text=*/u"Ugly Dog");
  AddStateToEffect(effect.get(),
                   /*state_value=*/static_cast<int>(HowCute::kTeddyBear),
                   /*label_text=*/u"Teddy Bear");
  AddStateToEffect(effect.get(),
                   /*state_value=*/static_cast<int>(HowCute::kZara),
                   /*label_text=*/u"Zara");
  AddEffect(std::move(effect));

  // Initialize click counts.
  for (int i = 0; i < static_cast<int>(HowCute::kMaxNumValues); ++i) {
    num_activations_for_testing_.push_back(0);
  }
}

SuperCutnessEffect::~SuperCutnessEffect() = default;

std::optional<int> SuperCutnessEffect::GetEffectState(VcEffectId effect_id) {
  if (has_invalid_effect_state_for_testing_) {
    return std::nullopt;
  }

  return static_cast<int>(HowCute::kTeddyBear);
}

void SuperCutnessEffect::OnEffectControlActivated(VcEffectId effect_id,
                                                  std::optional<int> state) {
  DCHECK(state.has_value());
  DCHECK(state.value() >= 0 &&
         state.value() < static_cast<int>(HowCute::kMaxNumValues));
  ++num_activations_for_testing_[state.value()];
}

int SuperCutnessEffect::GetNumActivationsForTesting(int state) {
  DCHECK(state >= 0 && state < static_cast<int>(HowCute::kMaxNumValues));
  return num_activations_for_testing_[state];
}

void SuperCutnessEffect::AddStateToEffect(VcHostedEffect* effect,
                                          int state_value,
                                          std::u16string label_text) {
  DCHECK(effect);
  effect->AddState(std::make_unique<VcEffectState>(
      /*icon=*/&ash::kPrivacyIndicatorsCameraIcon,
      /*label_text=*/label_text,
      /*accessible_name_id=*/IDS_PRIVACY_INDICATORS_STATUS_CAMERA,
      /*button_callback=*/
      base::BindRepeating(&SuperCutnessEffect::OnEffectControlActivated,
                          weak_factory_.GetWeakPtr(),
                          /*effect_id=*/VcEffectId::kTestEffect,
                          /*value=*/state_value),
      /*state=*/state_value));
}

// This registers/unregisters all effects owned by `EffectRepository`.
// Comment-out the `RegisterDelegate`/`UnregisterDelegate` calls for effects
// that are not needed e.g. to test `ash::video_conference::BubbleView` with
// only one or two registered effects.
EffectRepository::EffectRepository(
    ash::FakeVideoConferenceTrayController* controller)
    : controller_(controller),
      cat_ears_(std::make_unique<CatEarsEffect>()),
      dog_fur_(std::make_unique<DogFurEffect>()),
      spaceship_(std::make_unique<SpaceshipEffect>()),
      office_bunny_(std::make_unique<OfficeBunnyEffect>()),
      calm_forest_(std::make_unique<CalmForestEffect>()),
      stylish_kitchen_(std::make_unique<StylishKitchenEffect>()),
      long_text_label_effect_(
          std::make_unique<FakeLongTextLabelToggleEffect>()),
      shaggy_fur_(std::make_unique<ShaggyFurEffect>()),
      super_cuteness_(std::make_unique<SuperCutnessEffect>()) {
  DCHECK(controller_);
  if (features::IsVcControlsUiFakeEffectsEnabled()) {
    controller_->GetEffectsManager().RegisterDelegate(cat_ears_.get());
    controller_->GetEffectsManager().RegisterDelegate(dog_fur_.get());
    controller_->GetEffectsManager().RegisterDelegate(spaceship_.get());
    controller_->GetEffectsManager().RegisterDelegate(office_bunny_.get());
    controller_->GetEffectsManager().RegisterDelegate(calm_forest_.get());
    controller_->GetEffectsManager().RegisterDelegate(stylish_kitchen_.get());
    controller_->GetEffectsManager().RegisterDelegate(
        long_text_label_effect_.get());
    controller_->GetEffectsManager().RegisterDelegate(shaggy_fur_.get());
    controller_->GetEffectsManager().RegisterDelegate(super_cuteness_.get());
  }
}

EffectRepository::~EffectRepository() {
  if (features::IsVcControlsUiFakeEffectsEnabled()) {
    controller_->GetEffectsManager().UnregisterDelegate(cat_ears_.get());
    cat_ears_.reset();
    controller_->GetEffectsManager().UnregisterDelegate(dog_fur_.get());
    dog_fur_.reset();
    controller_->GetEffectsManager().UnregisterDelegate(spaceship_.get());
    spaceship_.reset();
    controller_->GetEffectsManager().UnregisterDelegate(office_bunny_.get());
    office_bunny_.reset();
    controller_->GetEffectsManager().UnregisterDelegate(calm_forest_.get());
    calm_forest_.reset();
    controller_->GetEffectsManager().UnregisterDelegate(stylish_kitchen_.get());
    stylish_kitchen_.reset();
    controller_->GetEffectsManager().UnregisterDelegate(
        long_text_label_effect_.get());
    long_text_label_effect_.reset();
    controller_->GetEffectsManager().UnregisterDelegate(shaggy_fur_.get());
    shaggy_fur_.reset();
    controller_->GetEffectsManager().UnregisterDelegate(super_cuteness_.get());
    super_cuteness_.reset();
  }
}

}  // namespace ash::fake_video_conference
