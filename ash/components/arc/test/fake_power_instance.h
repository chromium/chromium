// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_TEST_FAKE_POWER_INSTANCE_H_
#define ASH_COMPONENTS_ARC_TEST_FAKE_POWER_INSTANCE_H_

#include "ash/components/arc/mojom/power.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace arc {

class FakePowerInstance : public mojom::PowerInstance {
 public:
  FakePowerInstance();
  ~FakePowerInstance() override;

  FakePowerInstance(const FakePowerInstance&) = delete;
  FakePowerInstance& operator=(const FakePowerInstance&) = delete;

  bool interactive() const { return idle_state_ == mojom::IdleState::ACTIVE; }
  mojom::IdleState idle_state() const { return idle_state_; }
  int num_suspend() const { return num_suspend_; }
  int num_resume() const { return num_resume_; }
  double screen_brightness() const { return screen_brightness_; }
  int num_power_supply_info() const { return num_power_supply_info_; }
  int cpu_restriction_state_count() const {
    return cpu_restriction_state_count_;
  }
  mojom::CpuRestrictionState last_cpu_restriction_state() {
    return last_cpu_restriction_state_;
  }

  // Returns |suspend_callback_| and resets the member.
  SuspendCallback GetSuspendCallback();

  // mojom::PowerInstance overrides:
  void Init(mojo::PendingRemote<mojom::PowerHost> host_remote,
            InitCallback callback) override;
  void SetInteractiveDeprecated(bool enabled) override;
  void SetIdleState(mojom::IdleState state) override;
  void Suspend(SuspendCallback callback) override;
  void Resume() override;
  void UpdateScreenBrightnessSettings(double percent) override;
  void PowerSupplyInfoChanged() override;
  void GetWakefulnessMode(GetWakefulnessModeCallback callback) override;
  void OnCpuRestrictionChanged(
      mojom::CpuRestrictionState cpu_restriction_state) override;
  void OnBatterySaverModeStateChanged(
      mojom::BatterySaverModeStatePtr state) override;

 private:
  mojo::Remote<mojom::PowerHost> host_remote_;

  // Last state passed to SetInteractive().
  mojom::IdleState idle_state_ = mojom::IdleState::ACTIVE;

  // Number of calls to Suspend() and Resume().
  int num_suspend_ = 0;
  int num_resume_ = 0;

  // Last callback passed to Suspend().
  SuspendCallback suspend_callback_;

  // Last value passed to UpdateScreenBrightnessSettings().
  double screen_brightness_ = 0.0;

  // Number of calls to PowerSupplyInfoChanged().
  int num_power_supply_info_ = 0;

  // Number of calls to OnCpuRestrictionChanged().
  int cpu_restriction_state_count_ = 0;

  // Number of calls to OnBatterySaverModeStateChanged().
  int battery_saver_mode_state_count_ = 0;

  // Last passed argument to OnCpuRestrictionChanged().
  mojom::CpuRestrictionState last_cpu_restriction_state_ =
      mojom::CpuRestrictionState::CPU_RESTRICTION_FOREGROUND;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_TEST_FAKE_POWER_INSTANCE_H_
