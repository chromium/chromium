// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/scanner/scanner_delegate.h"
#include "ash/public/cpp/scanner/scanner_enums.h"
#include "ash/public/cpp/scanner/scanner_profile_scoped_delegate.h"
#include "ash/scanner/scanner_session.h"
#include "base/command_line.h"
#include "base/hash/sha1.h"

namespace ash {

namespace {

constexpr std::string_view kScannerKey(
    "\xF0\xC9\xFD\x45\x31\x92\x95\xAC\xBB\xD8\xD4\xB3\x5F\xF8\x98\x3B\x3B\x4F"
    "\x02\xF1",
    base::kSHA1Length);

}  // namespace

ScannerController::ScannerController(std::unique_ptr<ScannerDelegate> delegate)
    : delegate_(std::move(delegate)) {}

ScannerController::~ScannerController() = default;

bool ScannerController::IsEnabled() {
  // Command line looks like:
  //  out/Default/chrome --user-data-dir=/tmp/tmp123
  //  --scanner-update-key="INSERT KEY HERE" --enable-features=ScannerUpdate
  static const bool is_enabled =
      base::SHA1HashString(
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              switches::kScannerUpdateKey)) == kScannerKey;
  return is_enabled;
}

ScannerSession* ScannerController::StartNewSession() {
  ScannerProfileScopedDelegate* profile_scoped_delegate =
      delegate_->GetProfileScopedDelegate();
  scanner_session_ =
      profile_scoped_delegate &&
              profile_scoped_delegate->GetSystemState().status ==
                  ScannerStatus::kEnabled
          ? std::make_unique<ScannerSession>(profile_scoped_delegate)
          : nullptr;
  return scanner_session_.get();
}

void ScannerController::OnSessionUIClosed() {
  scanner_session_ = nullptr;
}

}  // namespace ash
