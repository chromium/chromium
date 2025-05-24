// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCANNER_SCANNER_COMMAND_DELEGATE_IMPL_H_
#define ASH_SCANNER_SCANNER_COMMAND_DELEGATE_IMPL_H_

#include "ash/ash_export.h"
#include "ash/scanner/scanner_command_delegate.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace ash {

class ScannerProfileScopedDelegate;

class ASH_EXPORT ScannerCommandDelegateImpl : public ScannerCommandDelegate {
 public:
  ScannerCommandDelegateImpl(ScannerProfileScopedDelegate* delegate);
  ScannerCommandDelegateImpl(const ScannerCommandDelegateImpl&) = delete;
  ScannerCommandDelegateImpl& operator=(const ScannerCommandDelegateImpl&) =
      delete;
  ~ScannerCommandDelegateImpl() override;

  // ScannerCommandDelegate:
  void OpenUrl(const GURL& url) override;
  drive::DriveServiceInterface* GetDriveService() override;
  google_apis::RequestSender* GetGoogleApisRequestSender() override;
  void SetClipboard(std::unique_ptr<ui::ClipboardData> data) override;
  base::WeakPtr<ScannerCommandDelegate> GetWeakPtr() override;

 private:
  const raw_ptr<ScannerProfileScopedDelegate> delegate_;

  base::WeakPtrFactory<ScannerCommandDelegateImpl> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SCANNER_SCANNER_COMMAND_DELEGATE_IMPL_H_
