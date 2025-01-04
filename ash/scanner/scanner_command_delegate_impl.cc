// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_command_delegate_impl.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/scanner/scanner_profile_scoped_delegate.h"
#include "base/check_deref.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/clipboard_non_backed.h"
#include "url/gurl.h"

namespace ash {

ScannerCommandDelegateImpl::ScannerCommandDelegateImpl(
    ScannerProfileScopedDelegate* delegate)
    : delegate_(delegate) {}

ScannerCommandDelegateImpl::~ScannerCommandDelegateImpl() = default;

void ScannerCommandDelegateImpl::OpenUrl(const GURL& url) {
  NewWindowDelegate::GetInstance()->OpenUrl(
      url, NewWindowDelegate::OpenUrlFrom::kUnspecified,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

drive::DriveServiceInterface* ScannerCommandDelegateImpl::GetDriveService() {
  return delegate_->GetDriveService();
}

google_apis::RequestSender*
ScannerCommandDelegateImpl::GetGoogleApisRequestSender() {
  return delegate_->GetGoogleApisRequestSender();
}

void ScannerCommandDelegateImpl::SetClipboard(
    std::unique_ptr<ui::ClipboardData> data) {
  CHECK_DEREF(ui::ClipboardNonBacked::GetForCurrentThread())
      .WriteClipboardData(std::move(data));
}

base::WeakPtr<ScannerCommandDelegate> ScannerCommandDelegateImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
