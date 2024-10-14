// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_controller.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/scanner/scanner_action.h"
#include "ash/public/cpp/scanner/scanner_delegate.h"
#include "ash/public/cpp/scanner/scanner_enums.h"
#include "ash/public/cpp/scanner/scanner_profile_scoped_delegate.h"
#include "ash/scanner/scanner_action_view_model.h"
#include "ash/scanner/scanner_session.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"

namespace ash {

namespace {

void OnActionsFetched(ScannerController::FetchActionsCallback callback,
                      std::vector<ScannerAction> actions) {
  std::vector<ScannerActionViewModel> action_view_models;

  action_view_models.reserve(actions.size());
  for (ScannerAction& action : actions) {
    action_view_models.emplace_back(std::move(action));
  }

  std::move(callback).Run(std::move(action_view_models));
}

}  // namespace

ScannerController::ScannerController(std::unique_ptr<ScannerDelegate> delegate)
    : delegate_(std::move(delegate)) {}

ScannerController::~ScannerController() = default;

bool ScannerController::IsEnabled() {
  return switches::IsScannerUpdateSecretKeyMatched();
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

void ScannerController::FetchActionsForImage(
    scoped_refptr<base::RefCountedMemory> jpeg_bytes,
    ScannerController::FetchActionsCallback callback) {
  if (!scanner_session_) {
    std::move(callback).Run({});
    return;
  }
  scanner_session_->FetchActionsForImage(
      jpeg_bytes, base::BindOnce(&OnActionsFetched, std::move(callback)));
}

void ScannerController::OnSessionUIClosed() {
  scanner_session_ = nullptr;
}

bool ScannerController::HasActiveSessionForTesting() const {
  return !!scanner_session_;
}

}  // namespace ash
