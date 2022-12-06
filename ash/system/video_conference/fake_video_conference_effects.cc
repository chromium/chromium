// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/fake_video_conference_effects.h"

#include "ash/strings/grit/ash_strings.h"

namespace ash::fake_video_conference {

SimpleToggleEffect::SimpleToggleEffect(
    const gfx::VectorIcon* icon,
    const std::u16string& label_text,
    int accessible_name_id,
    views::Button::PressedCallback button_callback)
    : effect_(VcEffectType::kToggle),
      state_(VcEffectState(
          /*value=*/VcEffectState::kUnusedId,
          /*icon=*/icon,
          /*label_text=*/label_text,
          /*accessible_name_id=*/accessible_name_id,
          /*button_callback=*/button_callback)) {
  effect_.AddState(&state_);
  AddEffect(&effect_);
}

CatEarsEffect::CatEarsEffect()
    : SimpleToggleEffect(
          /*icon=*/nullptr,
          /*label_text=*/u"Cat Ears",
          /*accessible_name_id=*/IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA,
          /*button_callback=*/
          base::BindRepeating(&CatEarsEffect::OnEffectControlActivated,
                              base::Unretained(this),
                              VcEffectState::kUnusedId,
                              0)) {}

int CatEarsEffect::GetEffectState(int effect_id) {
  return VcHostedEffect::kOff;
}

void CatEarsEffect::OnEffectControlActivated(int effect_id, int value) {}

DogFurEffect::DogFurEffect()
    : SimpleToggleEffect(
          /*icon=*/nullptr,
          /*label_text=*/u"Dog Fur",
          /*accessible_name_id=*/IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA,
          /*button_callback=*/
          base::BindRepeating(&DogFurEffect::OnEffectControlActivated,
                              base::Unretained(this),
                              VcEffectState::kUnusedId,
                              0)) {}

int DogFurEffect::GetEffectState(int effect_id) {
  return VcHostedEffect::kOff;
}

void DogFurEffect::OnEffectControlActivated(int effect_id, int value) {}

SpaceshipEffect::SpaceshipEffect()
    : SimpleToggleEffect(
          /*icon=*/nullptr,
          /*label_text=*/u"Spaceship",
          /*accessible_name_id=*/IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA,
          /*button_callback=*/
          base::BindRepeating(&SpaceshipEffect::OnEffectControlActivated,
                              base::Unretained(this),
                              VcEffectState::kUnusedId,
                              0)) {}

int SpaceshipEffect::GetEffectState(int effect_id) {
  return VcHostedEffect::kOff;
}

void SpaceshipEffect::OnEffectControlActivated(int effect_id, int value) {}

OfficeBunnyEffect::OfficeBunnyEffect()
    : SimpleToggleEffect(
          /*icon=*/nullptr,
          /*label_text=*/u"Office Bunny",
          /*accessible_name_id=*/IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA,
          /*button_callback=*/
          base::BindRepeating(&OfficeBunnyEffect::OnEffectControlActivated,
                              base::Unretained(this),
                              VcEffectState::kUnusedId,
                              0)) {}

int OfficeBunnyEffect::GetEffectState(int effect_id) {
  return VcHostedEffect::kOff;
}

void OfficeBunnyEffect::OnEffectControlActivated(int effect_id, int value) {}

CalmForestEffect::CalmForestEffect()
    : SimpleToggleEffect(
          /*icon=*/nullptr,
          /*label_text=*/u"Calm Forest",
          /*accessible_name_id=*/IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA,
          /*button_callback=*/
          base::BindRepeating(&CalmForestEffect::OnEffectControlActivated,
                              base::Unretained(this),
                              VcEffectState::kUnusedId,
                              0)) {}

int CalmForestEffect::GetEffectState(int effect_id) {
  return VcHostedEffect::kOff;
}

void CalmForestEffect::OnEffectControlActivated(int effect_id, int value) {}

StylishKitchenEffect::StylishKitchenEffect()
    : SimpleToggleEffect(
          /*icon=*/nullptr,
          /*label_text=*/u"Stylish Kitchen",
          /*accessible_name_id=*/IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA,
          /*button_callback=*/
          base::BindRepeating(&StylishKitchenEffect::OnEffectControlActivated,
                              base::Unretained(this),
                              VcEffectState::kUnusedId,
                              0)) {}

int StylishKitchenEffect::GetEffectState(int effect_id) {
  return VcHostedEffect::kOff;
}
void StylishKitchenEffect::OnEffectControlActivated(int effect_id, int value) {}

GreenhouseEffect::GreenhouseEffect()
    : SimpleToggleEffect(
          /*icon=*/nullptr,
          /*label_text=*/u"Greenhouse",
          /*accessible_name_id=*/IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA,
          /*button_callback=*/
          base::BindRepeating(&GreenhouseEffect::OnEffectControlActivated,
                              base::Unretained(this),
                              VcEffectState::kUnusedId,
                              0)) {}

int GreenhouseEffect::GetEffectState(int effect_id) {
  return VcHostedEffect::kOff;
}

void GreenhouseEffect::OnEffectControlActivated(int effect_id, int value) {}

// Delegate that hosts a set-value effect.

ShaggyFurEffect::ShaggyFurEffect()
    : effect_(VcEffectType::kSetValue),
      bald_state_(VcEffectState(
          /*value=*/static_cast<int>(FurShagginess::kBald),
          /*icon=*/nullptr,
          /*label_text=*/u"Bald",
          /*accessible_name_id=*/IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA,
          /*button_callback=*/
          base::BindRepeating(&ShaggyFurEffect::OnEffectControlActivated,
                              base::Unretained(this),
                              0,
                              static_cast<int>(FurShagginess::kBald)))),
      buzzcut_state_(VcEffectState(
          /*value=*/static_cast<int>(FurShagginess::kBuzzcut),
          /*icon=*/nullptr,
          /*label_text=*/u"Buzzcut",
          /*accessible_name_id=*/IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA,
          /*button_callback=*/
          base::BindRepeating(&ShaggyFurEffect::OnEffectControlActivated,
                              base::Unretained(this),
                              0,
                              static_cast<int>(FurShagginess::kBuzzcut)))),
      thick_state_(VcEffectState(
          /*value=*/static_cast<int>(FurShagginess::kThick),
          /*icon=*/nullptr,
          /*label_text=*/u"Thick",
          /*accessible_name_id=*/IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA,
          /*button_callback=*/
          base::BindRepeating(&ShaggyFurEffect::OnEffectControlActivated,
                              base::Unretained(this),
                              0,
                              static_cast<int>(FurShagginess::kThick)))) {
  effect_.AddState(&bald_state_);
  effect_.AddState(&buzzcut_state_);
  effect_.AddState(&thick_state_);
  effect_.set_label_text(u"Shaggy Fur");
  AddEffect(&effect_);
}

ShaggyFurEffect::~ShaggyFurEffect() = default;

int ShaggyFurEffect::GetEffectState(int effect_id) {
  return static_cast<int>(FurShagginess::kBuzzcut);
}

void ShaggyFurEffect::OnEffectControlActivated(int effect_id, int value) {}

}  // namespace ash::fake_video_conference
