// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_PERKS_DISCOVERY_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_PERKS_DISCOVERY_SCREEN_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chromeos/ash/components/growth/campaigns_manager.h"

namespace ash {
class PerksDiscoveryScreenView;

struct Illustration {
  std::string url;
  std::string width;
  std::string height;
};

struct Content {
  Content();
  ~Content();
  std::optional<Illustration> illustration;
};

// A wrapper struct for the OOBE Perks data.
struct SinglePerkDiscoveryPayload {
 public:
  explicit SinglePerkDiscoveryPayload(const base::Value::Dict& perk_data);
  ~SinglePerkDiscoveryPayload();
  SinglePerkDiscoveryPayload(const SinglePerkDiscoveryPayload& perk_data);

  std::string id;
  std::string title;
  std::string subtitle;
  std::string icon_url;
  Content content;

  base::Value::Dict primary_button;
  base::Value::Dict secondary_button;
};

// Controller for the new perks discovery screen.
class PerksDiscoveryScreen : public BaseScreen {
 public:
  using TView = PerksDiscoveryScreenView;

  enum class Result { kNext, kError, kNotApplicable };

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
  void GetOobePerksPayloadAndShow();

  std::vector<SinglePerkDiscoveryPayload> perks_data_;
  base::WeakPtr<PerksDiscoveryScreenView> view_;
  ScreenExitCallback exit_callback_;

  base::WeakPtrFactory<PerksDiscoveryScreen> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_PERKS_DISCOVERY_SCREEN_H_
