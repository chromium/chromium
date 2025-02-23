// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOBSTER_LOBSTER_SYSTEM_STATE_PROVIDER_H_
#define CHROME_BROWSER_ASH_LOBSTER_LOBSTER_SYSTEM_STATE_PROVIDER_H_

#include "ash/public/cpp/lobster/lobster_text_input_context.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/specialized_features/feature_access_checker.h"
#include "ui/base/ime/text_input_type.h"

class PrefService;

namespace ash {
struct LobsterSystemState;
}  // namespace ash

namespace signin {
class IdentityManager;
}

class LobsterSystemStateProvider {
 public:
  explicit LobsterSystemStateProvider(
      PrefService* pref,
      signin::IdentityManager* identity_manager);
  ~LobsterSystemStateProvider();

  ash::LobsterSystemState GetSystemState(
      const ash::LobsterTextInputContext& text_input_context);

 private:
  raw_ptr<PrefService> pref_;
  specialized_features::FeatureAccessChecker access_checker_;
};

#endif  // CHROME_BROWSER_ASH_LOBSTER_LOBSTER_SYSTEM_STATE_PROVIDER_H_
