// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_motd_page_handler_delegate.h"

#include <utility>

#include "base/check_deref.h"
#include "base/logging.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_installer.h"

namespace borealis {

BorealisMOTDPageHandlerDelegate::BorealisMOTDPageHandlerDelegate(
    BorealisFeatures* features,
    BorealisInstaller* installer)
    : features_(CHECK_DEREF(features)), installer_(CHECK_DEREF(installer)) {}

bool BorealisMOTDPageHandlerDelegate::IsBorealisInstalled() {
  return features_->IsEnabled();
}

void BorealisMOTDPageHandlerDelegate::UninstallBorealis() {
  VLOG(1) << "User initiated Borealis uninstall via MOTD dialog";
  installer_->Uninstall(base::BindOnce([](BorealisUninstallResult result) {
    if (result == BorealisUninstallResult::kSuccess) {
      VLOG(1) << "Borealis uninstalled successfully";
    } else {
      LOG(ERROR) << "Borealis uninstall failed: " << std::to_underlying(result);
    }
  }));
}

}  // namespace borealis
