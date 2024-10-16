// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_session.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/scanner/scanner_action.h"
#include "ash/public/cpp/scanner/scanner_enums.h"
#include "ash/public/cpp/scanner/scanner_profile_scoped_delegate.h"
#include "ash/scanner/scanner_action_view_model.h"
#include "ash/scanner/scanner_command_delegate.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/types/expected.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/clipboard_non_backed.h"
#include "url/gurl.h"

namespace ash {

ScannerSession::ScannerSession(ScannerProfileScopedDelegate* delegate)
    : delegate_(delegate) {}

ScannerSession::~ScannerSession() = default;

void ScannerSession::FetchActionsForImage(
    scoped_refptr<base::RefCountedMemory> jpeg_bytes,
    FetchActionsCallback callback) {
  delegate_->FetchActionsForImage(
      jpeg_bytes,
      base::BindOnce(&ScannerSession::OnActionsReturned,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ScannerSession::OnActionsReturned(
    FetchActionsCallback callback,
    base::expected<std::vector<ScannerAction>, ScannerError> returned) {
  if (!returned.has_value()) {
    // TODO(b/363100868): Handle error case
    std::move(callback).Run({});
    return;
  }

  std::vector<ScannerActionViewModel> action_view_models;

  action_view_models.reserve(returned->size());
  for (ScannerAction& action : *returned) {
    action_view_models.emplace_back(std::move(action),
                                    weak_ptr_factory_.GetWeakPtr());
  }

  std::move(callback).Run(std::move(action_view_models));
}

void ScannerSession::OpenUrl(const GURL& url) {
  NewWindowDelegate::GetInstance()->OpenUrl(
      url, NewWindowDelegate::OpenUrlFrom::kUnspecified,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

drive::DriveServiceInterface* ScannerSession::GetDriveService() {
  return delegate_->GetDriveService();
}

void ScannerSession::SetClipboard(std::unique_ptr<ui::ClipboardData> data) {
  CHECK_DEREF(ui::ClipboardNonBacked::GetForCurrentThread())
      .WriteClipboardData(std::move(data));

  // TODO: b/367871707 - Display a toast / notification if necessary.
}

}  // namespace ash
