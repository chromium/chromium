// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_session.h"

#include <memory>
#include <vector>

#include "ash/public/cpp/scanner/scanner_action.h"
#include "ash/public/cpp/scanner/scanner_enums.h"
#include "ash/public/cpp/scanner/scanner_profile_scoped_delegate.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/types/expected.h"

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
  // TODO(b/363100868): Handle error case
  std::move(callback).Run(returned.has_value() ? std::move(returned.value())
                                               : std::vector<ScannerAction>{});
}

}  // namespace ash
