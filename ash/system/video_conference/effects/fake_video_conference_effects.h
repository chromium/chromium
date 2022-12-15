// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_EFFECTS_FAKE_VIDEO_CONFERENCE_EFFECTS_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_EFFECTS_FAKE_VIDEO_CONFERENCE_EFFECTS_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_delegate.h"
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
  explicit SimpleToggleEffect(const std::u16string& label_text);
  // Allows setting `icon` and `accessible_name_id` if desired, for unit tests
  // or the emulator.
  SimpleToggleEffect(const std::u16string& label_text,
                     absl::optional<const gfx::VectorIcon*> icon,
                     absl::optional<int> accessible_name_id);

  SimpleToggleEffect(const SimpleToggleEffect&) = delete;
  SimpleToggleEffect& operator=(const SimpleToggleEffect&) = delete;

  ~SimpleToggleEffect() override = default;

  // VcEffectsDelegate:
  int GetEffectState(int effect_id) override;
  void OnEffectControlActivated(int effect_id, int value) override;

  int num_activations_for_testing() { return num_activations_for_testing_; }

 private:
  // Number of times the control has been activated, used by unit tests.
  int num_activations_for_testing_ = 0;
};

// Delegates that host a series of "fake" effects used in unit tests and the
// bubble (for the emulator).

class ASH_EXPORT CatEarsEffect : public SimpleToggleEffect {
 public:
  CatEarsEffect();

  CatEarsEffect(const CatEarsEffect&) = delete;
  CatEarsEffect& operator=(const CatEarsEffect&) = delete;

  ~CatEarsEffect() override = default;
};

class ASH_EXPORT DogFurEffect : public SimpleToggleEffect {
 public:
  DogFurEffect();

  DogFurEffect(const DogFurEffect&) = delete;
  DogFurEffect& operator=(const DogFurEffect&) = delete;

  ~DogFurEffect() override = default;
};

class ASH_EXPORT SpaceshipEffect : public SimpleToggleEffect {
 public:
  SpaceshipEffect();

  SpaceshipEffect(const SpaceshipEffect&) = delete;
  SpaceshipEffect& operator=(const SpaceshipEffect&) = delete;

  ~SpaceshipEffect() override = default;
};

class ASH_EXPORT OfficeBunnyEffect : public SimpleToggleEffect {
 public:
  OfficeBunnyEffect();

  OfficeBunnyEffect(const OfficeBunnyEffect&) = delete;
  OfficeBunnyEffect& operator=(const OfficeBunnyEffect&) = delete;

  ~OfficeBunnyEffect() override = default;
};

class ASH_EXPORT CalmForestEffect : public SimpleToggleEffect {
 public:
  CalmForestEffect();

  CalmForestEffect(const CalmForestEffect&) = delete;
  CalmForestEffect& operator=(const CalmForestEffect&) = delete;

  ~CalmForestEffect() override = default;
};

class ASH_EXPORT StylishKitchenEffect : public SimpleToggleEffect {
 public:
  StylishKitchenEffect();

  StylishKitchenEffect(const StylishKitchenEffect&) = delete;
  StylishKitchenEffect& operator=(const StylishKitchenEffect&) = delete;

  ~StylishKitchenEffect() override = default;
};

class ASH_EXPORT GreenhouseEffect : public SimpleToggleEffect {
 public:
  GreenhouseEffect();

  GreenhouseEffect(const GreenhouseEffect&) = delete;
  GreenhouseEffect& operator=(const GreenhouseEffect&) = delete;

  ~GreenhouseEffect() override = default;
};

// Delegate that hosts a set-value effect.

class ASH_EXPORT ShaggyFurEffect : public VcEffectsDelegate {
 public:
  enum class FurShagginess {
    kBald = 0,
    kBuzzcut = 1,
    kThick = 2,
    kMaxNumValues = 3,
  };

  ShaggyFurEffect();

  ShaggyFurEffect(const ShaggyFurEffect&) = delete;
  ShaggyFurEffect& operator=(const ShaggyFurEffect&) = delete;

  ~ShaggyFurEffect() override;

  // VcEffectsDelegate:
  int GetEffectState(int effect_id) override;
  void OnEffectControlActivated(int effect_id, int value) override;

  // Returns the number of times the button/state for `value` has been
  // activated.
  int GetNumActivationsForTesting(int value);

 private:
  // Number of times each value has been clicked, one count for each value in
  // `FurShagginess`.
  std::vector<int> num_activations_for_testing_;
};

class ASH_EXPORT SuperCutnessEffect : public VcEffectsDelegate {
 public:
  enum class HowCute {
    kUglyDog = 0,
    kTeddyBear = 1,
    kZara = 2,
    kInscrutable = 3,
    kMaxNumValues = 4,
  };

  SuperCutnessEffect();

  SuperCutnessEffect(const SuperCutnessEffect&) = delete;
  SuperCutnessEffect& operator=(const SuperCutnessEffect&) = delete;

  ~SuperCutnessEffect() override;

  // VcEffectsDelegate:
  int GetEffectState(int effect_id) override;
  void OnEffectControlActivated(int effect_id, int value) override;

  // Returns the number of times the button/state for `value` has been
  // activated.
  int GetNumActivationsForTesting(int value);

 private:
  // Number of times each value has been clicked, one count for each value in
  // `HowCute`.
  std::vector<int> num_activations_for_testing_;
};

}  // namespace ash::fake_video_conference

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_EFFECTS_FAKE_VIDEO_CONFERENCE_EFFECTS_H_
