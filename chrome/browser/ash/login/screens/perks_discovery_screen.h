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
#include "base/time/time.h"
#include "base/timer/timer.h"
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
  Content(const Content& content);
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
  std::optional<std::string> additional_text;
  std::string icon_url;
  Content content;

  base::Value::Dict primary_button;
  base::Value::Dict secondary_button;
};

// Controller for the new perks discovery screen.
class PerksDiscoveryScreen : public BaseScreen {
 public:
  using TView = PerksDiscoveryScreenView;

  enum class Result { kNext, kError, kTimeout, kNotApplicable };

  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/enums.xml, and should always reflect it (do not
  // change one without changing the other).  Entries should be never modified
  // or deleted. Only additions are possible.
  enum class PerksDiscoveryErrorReason {
    kNoCampaignManager = 0,
    kNoCampaign = 1,
    kNoCampaignID = 2,
    kNoPayload = 3,
    kMalformedPayload = 4,
    kNoUserProfile = 5,
    kNoActionFound = 6,
    kMaxValue = kNoActionFound
  };

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  PerksDiscoveryScreen(base::WeakPtr<PerksDiscoveryScreenView> view,
                       const ScreenExitCallback& exit_callback);

  PerksDiscoveryScreen(const PerksDiscoveryScreen&) = delete;
  PerksDiscoveryScreen& operator=(const PerksDiscoveryScreen&) = delete;

  ~PerksDiscoveryScreen() override;

  void set_exit_callback_for_testing(const ScreenExitCallback& callback) {
    exit_callback_ = callback;
  }

  const ScreenExitCallback& get_exit_callback_for_testing() {
    return exit_callback_;
  }

  void set_delay_for_overview_step_for_testing(base::TimeDelta delay) {
    delay_overview_step_ = delay;
  }

  static std::string GetResultString(Result result);

 private:
  // BaseScreen:
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;
  void GetOobePerksPayloadAndShow();
  void ShowOverviewStep();
  void ExitScreenTimeout();

  base::OneShotTimer delay_overview_timer_;
  base::TimeDelta delay_overview_step_ = base::Seconds(2);

  base::OneShotTimer timeout_overview_timer_;
  base::TimeDelta delay_exit_timeout_ = base::Minutes(1);

  // Called when the user finish all perks on the screen.
  void OnPerksSelectionFinished(const base::Value::List& selected_perks);

  // Forward the action to the campaign manager.
  void PerformButtonAction(const base::Value::Dict& button_data);

  std::vector<SinglePerkDiscoveryPayload> perks_data_;
  int campaign_id_;
  std::optional<int> group_id_;
  base::WeakPtr<PerksDiscoveryScreenView> view_;
  ScreenExitCallback exit_callback_;

  base::WeakPtrFactory<PerksDiscoveryScreen> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_PERKS_DISCOVERY_SCREEN_H_
