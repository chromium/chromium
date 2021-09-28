// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/fingerprint_auth_model.h"

#include "ash/login/ui/auth_icon_view.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"

// TODO(crbug.com/1233614): This stub will be replaced with a full
// implementation as part of the Smart Lock UI revamp.

namespace ash {

FingerprintAuthModel::FingerprintAuthModel() = default;

FingerprintAuthModel::~FingerprintAuthModel() = default;

AuthFactorModel::AuthFactorState FingerprintAuthModel::GetAuthFactorState() {
  // TODO(crbug.com/1233614): Calculate the correct AuthFactorState based on the
  // current FingerprintState.
  return AuthFactorState::kUnavailable;
}

AuthFactorType FingerprintAuthModel::GetType() {
  return AuthFactorType::kFingerprint;
}

std::u16string FingerprintAuthModel::GetLabel() {
  // TODO(crbug.com/1233614): Calculate the correct label based on the current
  // FingerprintState.
  return l10n_util::GetStringUTF16(IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_AVAILABLE);
}

bool FingerprintAuthModel::ShouldAnnounceLabel() {
  // TODO(crbug.com/1233614): Correctly choose which labels to announce in order
  // to match the behavior of the existing Fingerprint implementation.
  return false;
}

void FingerprintAuthModel::UpdateIcon(AuthIconView* icon_view) {
  // TODO(crbug.com/1233614): Show the correct icon/animation depending on the
  // current FingerprintState.
  icon_view->SetIcon(kLockScreenFingerprintIcon);
}

}  // namespace ash
