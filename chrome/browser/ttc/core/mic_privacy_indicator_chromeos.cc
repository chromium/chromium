// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/mic_privacy_indicator_chromeos.h"

#include "ash/system/privacy/privacy_indicators_controller.h"
#include "base/check_is_test.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace ttc {

MicPrivacyIndicator::MicPrivacyIndicator() = default;

MicPrivacyIndicator::~MicPrivacyIndicator() {
  SetCapturing(false);
}

void MicPrivacyIndicator::SetCapturing(bool capturing) {
  if (capturing_ == capturing) {
    return;
  }
  capturing_ = capturing;
  // Null without an Ash shell, which only happens in unit tests.
  ash::PrivacyIndicatorsController* controller =
      ash::PrivacyIndicatorsController::Get();
  if (!controller) {
    CHECK_IS_TEST();
    return;
  }
  controller->UpdatePrivacyIndicators(
      ash::kTtcPrivacyIndicatorsAppId,
      l10n_util::GetStringUTF16(IDS_TTC_ENTRYPOINT_LABEL),
      /*is_camera_used=*/false, /*is_microphone_used=*/capturing,
      base::MakeRefCounted<ash::PrivacyIndicatorsNotificationDelegate>(),
      ash::PrivacyIndicatorsSource::kApps);
}

}  // namespace ttc
