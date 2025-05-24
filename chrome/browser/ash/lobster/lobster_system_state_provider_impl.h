// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOBSTER_LOBSTER_SYSTEM_STATE_PROVIDER_IMPL_H_
#define CHROME_BROWSER_ASH_LOBSTER_LOBSTER_SYSTEM_STATE_PROVIDER_IMPL_H_

#include "ash/public/cpp/lobster/lobster_system_state.h"
#include "ash/public/cpp/lobster/lobster_text_input_context.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/lobster/lobster_system_state_provider.h"
#include "chromeos/ash/components/specialized_features/feature_access_checker.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/display/display_observer.h"

class PrefService;

namespace signin {
class IdentityManager;
}

namespace display {
enum class TabletState;
}  // namespace display

class LobsterSystemStateProviderImpl : public LobsterSystemStateProvider,
                                       public display::DisplayObserver {
 public:
  explicit LobsterSystemStateProviderImpl(
      PrefService* pref,
      signin::IdentityManager* identity_manager,
      bool is_in_demo_mode);

  ~LobsterSystemStateProviderImpl() override;

  ash::LobsterSystemState GetSystemState(
      const ash::LobsterTextInputContext& text_input_context) override;

  // display::DisplayObserver overrides
  void OnDisplayTabletStateChanged(display::TabletState state) override;

 private:
  raw_ptr<PrefService> pref_;
  specialized_features::FeatureAccessChecker access_checker_;
  bool is_in_tablet_mode_ = false;
  bool is_in_demo_mode_ = false;
  display::ScopedDisplayObserver display_observer_{this};
};

#endif  // CHROME_BROWSER_ASH_LOBSTER_LOBSTER_SYSTEM_STATE_PROVIDER_IMPL_H_
