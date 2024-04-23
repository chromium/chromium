// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shimless_rma/backend/fake_shimless_rma_delegate.h"

#include <string>
#include <utility>

#include "base/functional/callback.h"

namespace ash::shimless_rma {

FakeShimlessRmaDelegate::FakeShimlessRmaDelegate() = default;

FakeShimlessRmaDelegate::~FakeShimlessRmaDelegate() = default;

void FakeShimlessRmaDelegate::GenerateQrCode(
    const std::string& url,
    base::OnceCallback<void(const std::string& qr_code_image)> callback) {
  std::move(callback).Run(url);
}

void FakeShimlessRmaDelegate::PrepareDiagnosticsAppBrowserContext(
    const base::FilePath& crx_path,
    const base::FilePath& swbn_path,
    PrepareDiagnosticsAppBrowserContextCallback callback) {
  last_load_crx_path_ = crx_path;
  last_load_swbn_path_ = swbn_path;

  std::move(callback).Run(prepare_diagnostics_app_result_);
}

bool FakeShimlessRmaDelegate::IsChromeOSSystemExtensionProvider(
    const std::string& manufacturer) {
  return is_chromeos_system_extension_provider_;
}

base::WeakPtr<ShimlessRmaDelegate> FakeShimlessRmaDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash::shimless_rma
