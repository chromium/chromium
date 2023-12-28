// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_EFFECTS_FAKE_VIDEO_CONFERENCE_EFFECTS_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_EFFECTS_FAKE_VIDEO_CONFERENCE_EFFECTS_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_delegate.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/controls/button/button.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace ash {

class FakeVideoConferenceTrayController;
enum class VcEffectId;

namespace fake_video_conference {

// A convenience base class, for creating a delegate that hosts the simplest
// type of effect there is i.e. a toggle with only one state.
class SimpleToggleEffect : public VcEffectsDelegate {
 public:
  // Simplest of all, no `icon` and no `accessible_name_id`, for unit tests.
  explicit SimpleToggleEffect(const std::u16string& label_text);
  // Allows setting `icon` and `accessible_name_id` if desired, for unit tests
  // or the emulator.
  SimpleToggleEffect(const std::u16string& label_text,
                     std::optional<const gfx::VectorIcon*> icon,
                     std::optional<int> accessible_name_id);

  SimpleToggleEffect(const SimpleToggleEffect&) = delete;
  SimpleToggleEffect& operator=(const SimpleToggleEffect&) = delete;

  ~SimpleToggleEffect() override;

  // VcEffectsDelegate:
  std::optional<int> GetEffectState(VcEffectId effect_id) override;
  void OnEffectControlActivated(VcEffectId effect_id,
                                std::optional<int> state) override;

  int num_activations_for_testing() { return num_activations_for_testing_; }

 private:
  // Number of times the control has been activated, used by unit tests.
  int num_activations_for_testing_ = 0;

  base::WeakPtrFactory<SimpleToggleEffect> weak_factory_{this};
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

// A fake toggle effect with long text label (used to text multi-line label in
// the toggle effect button).
class ASH_EXPORT FakeLongTextLabelToggleEffect : public SimpleToggleEffect {
 public:
  FakeLongTextLabelToggleEffect();

  FakeLongTextLabelToggleEffect(const FakeLongTextLabelToggleEffect&) = delete;
  FakeLongTextLabelToggleEffect& operator=(
      const FakeLongTextLabelToggleEffect&) = delete;

  ~FakeLongTextLabelToggleEffect() override = default;
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
  std::optional<int> GetEffectState(VcEffectId effect_id) override;
  void OnEffectControlActivated(VcEffectId effect_id,
                                std::optional<int> state) override;

  // Returns the number of times the button for `state_value` has been
  // activated.
  int GetNumActivationsForTesting(int state_value);

 private:
  // Adds a `std::unique_ptr<VcEffectState>` to `effect`.
  void AddStateToEffect(VcHostedEffect* effect,
                        int state_value,
                        std::u16string label_text);

  // Number of times each value has been clicked, one count for each value in
  // `FurShagginess`.
  std::vector<int> num_activations_for_testing_;

  base::WeakPtrFactory<ShaggyFurEffect> weak_factory_{this};
};

class ASH_EXPORT SuperCutnessEffect : public VcEffectsDelegate {
 public:
  enum class HowCute {
    kUglyDog = 0,
    kTeddyBear = 1,
    kZara = 2,
    kMaxNumValues = 3,
  };

  SuperCutnessEffect();

  SuperCutnessEffect(const SuperCutnessEffect&) = delete;
  SuperCutnessEffect& operator=(const SuperCutnessEffect&) = delete;

  ~SuperCutnessEffect() override;

  // VcEffectsDelegate:
  std::optional<int> GetEffectState(VcEffectId effect_id) override;
  void OnEffectControlActivated(VcEffectId effect_id,
                                std::optional<int> state) override;

  // Returns the number of times the button for `state` has been activated.
  int GetNumActivationsForTesting(int state);

  void set_has_invalid_effect_state_for_testing(bool has_invalid_state) {
    has_invalid_effect_state_for_testing_ = has_invalid_state;
  }
  bool has_invalid_effect_state_for_testing() {
    return has_invalid_effect_state_for_testing_;
  }

 private:
  // Adds a `std::unique_ptr<VcEffectState>` to `effect`.
  void AddStateToEffect(VcHostedEffect* effect,
                        int state_value,
                        std::u16string label_text);

  // Number of times each value has been clicked, one count for each value in
  // `HowCute`.
  std::vector<int> num_activations_for_testing_;

  // Set to 'true' for testing the case where a valid effect state cannot be
  // obtained.
  bool has_invalid_effect_state_for_testing_;

  base::WeakPtrFactory<SuperCutnessEffect> weak_factory_{this};
};

// A simple residence for any fake effects used for testing. For all of these
// fake effects to be registered, the feature `VcControlsUiFakeEffects` must be
// enabled.
class EffectRepository {
 public:
  explicit EffectRepository(FakeVideoConferenceTrayController* controller);

  EffectRepository(const EffectRepository&) = delete;
  EffectRepository& operator=(const EffectRepository&) = delete;

  ~EffectRepository();

 private:
  raw_ptr<FakeVideoConferenceTrayController> controller_;
  std::unique_ptr<CatEarsEffect> cat_ears_;
  std::unique_ptr<DogFurEffect> dog_fur_;
  std::unique_ptr<SpaceshipEffect> spaceship_;
  std::unique_ptr<OfficeBunnyEffect> office_bunny_;
  std::unique_ptr<CalmForestEffect> calm_forest_;
  std::unique_ptr<StylishKitchenEffect> stylish_kitchen_;
  std::unique_ptr<FakeLongTextLabelToggleEffect> long_text_label_effect_;
  std::unique_ptr<ShaggyFurEffect> shaggy_fur_;
  std::unique_ptr<SuperCutnessEffect> super_cuteness_;
};

}  // namespace fake_video_conference

}  // namespace ash

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_EFFECTS_FAKE_VIDEO_CONFERENCE_EFFECTS_H_
