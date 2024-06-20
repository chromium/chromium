// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_PERKS_DISCOVERY_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_PERKS_DISCOVERY_SCREEN_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

namespace ash {
class PerksDiscoveryScreenView;

// Controller for the new perks discovery screen.
class PerksDiscoveryScreen : public BaseScreen {
 public:
  using TView = PerksDiscoveryScreenView;

  enum class Result { kNext, kNotApplicable };

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  PerksDiscoveryScreen(base::WeakPtr<PerksDiscoveryScreenView> view,
                       const ScreenExitCallback& exit_callback);

  PerksDiscoveryScreen(const PerksDiscoveryScreen&) = delete;
  PerksDiscoveryScreen& operator=(const PerksDiscoveryScreen&) = delete;

  ~PerksDiscoveryScreen() override;

  static std::string GetResultString(Result result);

 private:
  // BaseScreen:
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  base::WeakPtr<PerksDiscoveryScreenView> view_;
  ScreenExitCallback exit_callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_PERKS_DISCOVERY_SCREEN_H_
