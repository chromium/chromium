// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanner/chrome_scanner_delegate.h"

#include "ash/public/cpp/scanner/scanner_profile_scoped_delegate.h"
#include "ash/scanner/scanner_controller.h"
#include "chrome/browser/ash/scanner/scanner_keyed_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"

ChromeScannerDelegate::ChromeScannerDelegate() = default;
ChromeScannerDelegate::~ChromeScannerDelegate() = default;

ash::ScannerProfileScopedDelegate*
ChromeScannerDelegate::GetProfileScopedDelegate() {
  return ScannerKeyedServiceFactory::GetForProfile(
      ProfileManager::GetActiveUserProfile());
}
