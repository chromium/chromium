// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/fake_video_conference_effects.h"

#include <string>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/video_conference/video_conference_tray_effects_manager_types.h"
#include "base/functional/bind.h"
#include "ui/views/controls/button/button.h"

namespace ash::fake_video_conference {

SimpleToggleEffect::SimpleToggleEffect(
    const std::u16string& label_text,
    views::Button::PressedCallback button_callback)
    : SimpleToggleEffect(/*icon=*/nullptr,
                         /*label_text=*/label_text,
                         /*accessible_name_id=*/-1,
                         /*button_callback=*/button_callback) {}

SimpleToggleEffect::SimpleToggleEffect(
    const gfx::VectorIcon* icon,
    const std::u16string& label_text,
    int accessible_name_id,
    views::Button::PressedCallback button_callback) {
  std::unique_ptr<VcHostedEffect> effect =
      std::make_unique<VcHostedEffect>(VcEffectType::kToggle);
  std::unique_ptr<VcEffectState> state = std::make_unique<VcEffectState>(
      icon, label_text, accessible_name_id, button_callback);
  effect->AddState(std::move(state));
  AddEffect(std::move(effect));
}

CatEarsEffect::CatEarsEffect()
    : SimpleToggleEffect(
          /*label_text=*/u"Cat Ears",
          /*button_callback=*/
          base::BindRepeating(&CatEarsEffect::OnEffectControlActivated,
                              base::Unretained(this),
                              /*effect_id=*/VcEffectState::kUnusedId,
                              /*value=*/0)) {}

int CatEarsEffect::GetEffectState(int effect_id) {
  return VcHostedEffect::kOff;
}

void CatEarsEffect::OnEffectControlActivated(int effect_id, int value) {}

DogFurEffect::DogFurEffect()
    : SimpleToggleEffect(
          /*label_text=*/u"Dog Fur",
          /*button_callback=*/
          base::BindRepeating(&DogFurEffect::OnEffectControlActivated,
                              base::Unretained(this),
                              /*effect_id=*/VcEffectState::kUnusedId,
                              /*value=*/0)) {}

int DogFurEffect::GetEffectState(int effect_id) {
  return VcHostedEffect::kOff;
}

void DogFurEffect::OnEffectControlActivated(int effect_id, int value) {}

SpaceshipEffect::SpaceshipEffect()
    : SimpleToggleEffect(
          /*label_text=*/u"Spaceship",
          /*button_callback=*/
          base::BindRepeating(&SpaceshipEffect::OnEffectControlActivated,
                              base::Unretained(this),
                              /*effect_id=*/VcEffectState::kUnusedId,
                              /*value=*/0)) {}

int SpaceshipEffect::GetEffectState(int effect_id) {
  return VcHostedEffect::kOff;
}

void SpaceshipEffect::OnEffectControlActivated(int effect_id, int value) {}

OfficeBunnyEffect::OfficeBunnyEffect()
    : SimpleToggleEffect(
          /*label_text=*/u"Office Bunny",
          /*button_callback=*/
          base::BindRepeating(&OfficeBunnyEffect::OnEffectControlActivated,
                              base::Unretained(this),
                              /*effect_id=*/VcEffectState::kUnusedId,
                              /*value=*/0)) {}

int OfficeBunnyEffect::GetEffectState(int effect_id) {
  return VcHostedEffect::kOff;
}

void OfficeBunnyEffect::OnEffectControlActivated(int effect_id, int value) {}

CalmForestEffect::CalmForestEffect()
    : SimpleToggleEffect(
          /*label_text=*/u"Calm Forest",
          /*button_callback=*/
          base::BindRepeating(&CalmForestEffect::OnEffectControlActivated,
                              base::Unretained(this),
                              /*effect_id=*/VcEffectState::kUnusedId,
                              /*value=*/0)) {}

int CalmForestEffect::GetEffectState(int effect_id) {
  return VcHostedEffect::kOff;
}

void CalmForestEffect::OnEffectControlActivated(int effect_id, int value) {}

StylishKitchenEffect::StylishKitchenEffect()
    : SimpleToggleEffect(
          /*label_text=*/u"Stylish Kitchen",
          /*button_callback=*/
          base::BindRepeating(&StylishKitchenEffect::OnEffectControlActivated,
                              base::Unretained(this),
                              /*effect_id=*/VcEffectState::kUnusedId,
                              /*value=*/0)) {}

int StylishKitchenEffect::GetEffectState(int effect_id) {
  return VcHostedEffect::kOff;
}
void StylishKitchenEffect::OnEffectControlActivated(int effect_id, int value) {}

GreenhouseEffect::GreenhouseEffect()
    : SimpleToggleEffect(
          /*label_text=*/u"Greenhouse",
          /*button_callback=*/
          base::BindRepeating(&GreenhouseEffect::OnEffectControlActivated,
                              base::Unretained(this),
                              /*effect_id=*/VcEffectState::kUnusedId,
                              /*value=*/0)) {}

int GreenhouseEffect::GetEffectState(int effect_id) {
  return VcHostedEffect::kOff;
}

void GreenhouseEffect::OnEffectControlActivated(int effect_id, int value) {}

// Delegate that hosts a set-value effect.

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
  AddEffect(std::move(effect));
}

ShaggyFurEffect::~ShaggyFurEffect() = default;

int ShaggyFurEffect::GetEffectState(int effect_id) {
  return static_cast<int>(FurShagginess::kBuzzcut);
}

void ShaggyFurEffect::OnEffectControlActivated(int effect_id, int value) {}

}  // namespace ash::fake_video_conference
