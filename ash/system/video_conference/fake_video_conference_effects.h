// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_FAKE_VIDEO_CONFERENCE_EFFECTS_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_FAKE_VIDEO_CONFERENCE_EFFECTS_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/system/video_conference/video_conference_tray_effects_delegate.h"
#include "ui/views/controls/button/button.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace ash::fake_video_conference {

// A convenience base class, for creating a delegate that hosts the simplest
// type of effect there is i.e. a toggle with only one state.
class SimpleToggleEffect : public VcEffectsDelegate {
 public:
  // Simplest of all, no `icon` and no `accessible_name_id`, for unit tests.
  SimpleToggleEffect(const std::u16string& label_text,
                     views::Button::PressedCallback button_callback);
  // Allows setting `icon` if desired, and requires `accessible_name_id`, for
  // unit tests or the emulator.
  SimpleToggleEffect(const gfx::VectorIcon* icon,
                     const std::u16string& label_text,
                     int accessible_name_id,
                     views::Button::PressedCallback button_callback);

  SimpleToggleEffect(const SimpleToggleEffect&) = delete;
  SimpleToggleEffect& operator=(const SimpleToggleEffect&) = delete;

  ~SimpleToggleEffect() override = default;
};

// Delegates that host a series of "fake" effects used in unit tests and the
// bubble (for the emulator).

class ASH_EXPORT CatEarsEffect : public SimpleToggleEffect {
 public:
  CatEarsEffect();

  CatEarsEffect(const CatEarsEffect&) = delete;
  CatEarsEffect& operator=(const CatEarsEffect&) = delete;

  ~CatEarsEffect() override = default;

  // VcEffectsDelegate:
  int GetEffectState(int effect_id) override;
  void OnEffectControlActivated(int effect_id, int value) override;
};

class ASH_EXPORT DogFurEffect : public SimpleToggleEffect {
 public:
  DogFurEffect();

  DogFurEffect(const DogFurEffect&) = delete;
  DogFurEffect& operator=(const DogFurEffect&) = delete;

  ~DogFurEffect() override = default;

  // VcEffectsDelegate:
  int GetEffectState(int effect_id) override;
  void OnEffectControlActivated(int effect_id, int value) override;
};

class ASH_EXPORT SpaceshipEffect : public SimpleToggleEffect {
 public:
  SpaceshipEffect();

  SpaceshipEffect(const SpaceshipEffect&) = delete;
  SpaceshipEffect& operator=(const SpaceshipEffect&) = delete;

  ~SpaceshipEffect() override = default;

  // VcEffectsDelegate:
  int GetEffectState(int effect_id) override;
  void OnEffectControlActivated(int effect_id, int value) override;
};

class ASH_EXPORT OfficeBunnyEffect : public SimpleToggleEffect {
 public:
  OfficeBunnyEffect();

  OfficeBunnyEffect(const OfficeBunnyEffect&) = delete;
  OfficeBunnyEffect& operator=(const OfficeBunnyEffect&) = delete;

  ~OfficeBunnyEffect() override = default;

  // VcEffectsDelegate:
  int GetEffectState(int effect_id) override;
  void OnEffectControlActivated(int effect_id, int value) override;
};

class ASH_EXPORT CalmForestEffect : public SimpleToggleEffect {
 public:
  CalmForestEffect();

  CalmForestEffect(const CalmForestEffect&) = delete;
  CalmForestEffect& operator=(const CalmForestEffect&) = delete;

  ~CalmForestEffect() override = default;

  // VcEffectsDelegate:
  int GetEffectState(int effect_id) override;
  void OnEffectControlActivated(int effect_id, int value) override;
};

class ASH_EXPORT StylishKitchenEffect : public SimpleToggleEffect {
 public:
  StylishKitchenEffect();

  StylishKitchenEffect(const StylishKitchenEffect&) = delete;
  StylishKitchenEffect& operator=(const StylishKitchenEffect&) = delete;

  ~StylishKitchenEffect() override = default;

  // VcEffectsDelegate:
  int GetEffectState(int effect_id) override;
  void OnEffectControlActivated(int effect_id, int value) override;
};

class ASH_EXPORT GreenhouseEffect : public SimpleToggleEffect {
 public:
  GreenhouseEffect();

  GreenhouseEffect(const GreenhouseEffect&) = delete;
  GreenhouseEffect& operator=(const GreenhouseEffect&) = delete;

  ~GreenhouseEffect() override = default;

  // VcEffectsDelegate:
  int GetEffectState(int effect_id) override;
  void OnEffectControlActivated(int effect_id, int value) override;
};

// Delegate that hosts a set-value effect.

class ASH_EXPORT ShaggyFurEffect : public VcEffectsDelegate {
 public:
  enum class FurShagginess {
    kBald = 0,
    kBuzzcut = 1,
    kThick = 2,
  };

  ShaggyFurEffect();

  ShaggyFurEffect(const ShaggyFurEffect&) = delete;
  ShaggyFurEffect& operator=(const ShaggyFurEffect&) = delete;

  ~ShaggyFurEffect() override;

  // VcEffectsDelegate:
  int GetEffectState(int effect_id) override;
  void OnEffectControlActivated(int effect_id, int value) override;
};

}  // namespace ash::fake_video_conference

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_FAKE_VIDEO_CONFERENCE_EFFECTS_H_
