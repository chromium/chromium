// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanner/chrome_scanner_delegate.h"

#include <utility>

#include "ash/public/cpp/scanner/scanner_feedback_info.h"
#include "ash/public/cpp/scanner/scanner_profile_scoped_delegate.h"
#include "ash/scanner/scanner_controller.h"
#include "chrome/browser/ash/scanner/scanner_keyed_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/scanner_feedback_dialog/scanner_feedback_dialog.h"

ChromeScannerDelegate::ChromeScannerDelegate() = default;
ChromeScannerDelegate::~ChromeScannerDelegate() = default;

ash::ScannerProfileScopedDelegate*
ChromeScannerDelegate::GetProfileScopedDelegate() {
  return ScannerKeyedServiceFactory::GetForProfile(
      ProfileManager::GetActiveUserProfile());
}

void ChromeScannerDelegate::OpenFeedbackDialog(
    ash::ScannerFeedbackInfo feedback_info) {
  auto* dialog = new ash::ScannerFeedbackDialog(std::move(feedback_info));
  dialog->ShowSystemDialog();
}
