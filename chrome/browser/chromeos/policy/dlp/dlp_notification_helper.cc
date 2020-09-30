// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_notification_helper.h"

#include "ash/public/cpp/toast_data.h"
#include "ash/public/cpp/toast_manager.h"
#include "base/optional.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace policy {

namespace {

const char kPrintToastId[] = "print_dlp_blocked";
constexpr int kPrintToastDurationMs = 2500;

}  // namespace

void ShowDlpPrintDisabledToast() {
  ash::ToastData toast(
      kPrintToastId, l10n_util::GetStringUTF16(IDS_POLICY_DLP_PRINTING_BLOCKED),
      kPrintToastDurationMs, base::nullopt);
  toast.is_managed = true;

  ash::ToastManager::Get()->Show(toast);
}

}  // namespace policy
