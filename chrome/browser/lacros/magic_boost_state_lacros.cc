// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/magic_boost_state_lacros.h"

#include "base/functional/callback_forward.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "chromeos/lacros/lacros_service.h"

namespace chromeos {

namespace {

void SetPref(crosapi::mojom::PrefPath path, base::Value value) {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service ||
      !lacros_service->IsAvailable<crosapi::mojom::Prefs>()) {
    LOG(WARNING) << "crosapi: Prefs API not available";
    return;
  }
  lacros_service->GetRemote<crosapi::mojom::Prefs>()->SetPref(
      path, std::move(value), base::DoNothing());
}

}  // namespace

MagicBoostStateLacros::MagicBoostStateLacros()
    : consent_status_observer_(
          crosapi::mojom::PrefPath::kHMRConsentStatus,
          base::BindRepeating(&MagicBoostStateLacros::OnHMRConsentStatusUpdated,
                              base::Unretained(this))),
      consent_window_dismiss_count_observer_(
          crosapi::mojom::PrefPath::kHMRConsentWindowDismissCount,
          base::BindRepeating(
              &MagicBoostStateLacros::OnHMRConsentWindowDismissCountUpdated,
              base::Unretained(this))) {}

MagicBoostStateLacros::~MagicBoostStateLacros() = default;

bool MagicBoostStateLacros::IsMagicBoostAvailable() {
  // Magic Boost does not work in Lacros.
  return false;
}

bool MagicBoostStateLacros::CanShowNoticeBannerForHMR() {
  // Magic Boost does not work in Lacros.
  return false;
}

int32_t MagicBoostStateLacros::AsyncIncrementHMRConsentWindowDismissCount() {
  int count = hmr_consent_window_dismiss_count() + 1;
  SetPref(crosapi::mojom::PrefPath::kHMRConsentWindowDismissCount,
          base::Value(count));
  return count;
}

void MagicBoostStateLacros::AsyncWriteConsentStatus(
    chromeos::HMRConsentStatus consent_status) {
  SetPref(crosapi::mojom::PrefPath::kHMRConsentStatus,
          base::Value(base::to_underlying(consent_status)));
}

void MagicBoostStateLacros::MagicBoostStateLacros::OnHMRConsentStatusUpdated(
    base::Value value) {
  auto consent_status = static_cast<chromeos::HMRConsentStatus>(value.GetInt());
  UpdateHMRConsentStatus(consent_status);
}

void MagicBoostStateLacros::OnHMRConsentWindowDismissCountUpdated(
    base::Value value) {
  UpdateHMRConsentWindowDismissCount(value.GetInt());
}

void MagicBoostStateLacros::AsyncWriteHMREnabled(bool enabled) {
  SetPref(crosapi::mojom::PrefPath::kHmrEnabled, base::Value(enabled));
}

void MagicBoostStateLacros::DisableOrcaFeature() {}

}  // namespace chromeos
