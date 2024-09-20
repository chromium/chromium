// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_MAGIC_BOOST_STATE_LACROS_H_
#define CHROME_BROWSER_LACROS_MAGIC_BOOST_STATE_LACROS_H_

#include "base/functional/callback_forward.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "chromeos/lacros/crosapi_pref_observer.h"

namespace chromeos {

// A class that holds MagicBoost related prefs and states.
class MagicBoostStateLacros : public MagicBoostState {
 public:
  MagicBoostStateLacros();
  MagicBoostStateLacros(const MagicBoostStateLacros&) = delete;
  MagicBoostStateLacros& operator=(const MagicBoostStateLacros&) = delete;
  ~MagicBoostStateLacros() override;

  // MagicBoostState:
  bool IsMagicBoostAvailable() override;
  bool CanShowNoticeBannerForHMR() override;
  int32_t AsyncIncrementHMRConsentWindowDismissCount() override;
  void AsyncWriteConsentStatus(
      chromeos::HMRConsentStatus consent_status) override;
  void AsyncWriteHMREnabled(bool enabled) override;
  void DisableOrcaFeature() override;

 private:
  void OnHMRConsentStatusUpdated(base::Value value);
  void OnHMRConsentWindowDismissCountUpdated(base::Value value);

  // Observers to track pref changes from ash.
  CrosapiPrefObserver consent_status_observer_;
  CrosapiPrefObserver consent_window_dismiss_count_observer_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_LACROS_MAGIC_BOOST_STATE_LACROS_H_
