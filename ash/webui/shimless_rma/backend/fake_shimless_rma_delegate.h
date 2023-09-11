// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SHIMLESS_RMA_BACKEND_FAKE_SHIMLESS_RMA_DELEGATE_H_
#define ASH_WEBUI_SHIMLESS_RMA_BACKEND_FAKE_SHIMLESS_RMA_DELEGATE_H_

#include "ash/webui/shimless_rma/backend/shimless_rma_delegate.h"

namespace ash::shimless_rma {

class FakeShimlessRmaDelegate : public ShimlessRmaDelegate {
 public:
  FakeShimlessRmaDelegate();
  FakeShimlessRmaDelegate(const FakeShimlessRmaDelegate&) = delete;
  FakeShimlessRmaDelegate& operator=(const FakeShimlessRmaDelegate&) = delete;
  ~FakeShimlessRmaDelegate() override;

  void ExitRmaThenRestartChrome() override {}
  void ShowDiagnosticsDialog() override {}
  void RefreshAccessibilityManagerProfile() override {}
  void GenerateQrCode(const std::string& url,
                      base::OnceCallback<void(const std::string& qr_code_image)>
                          callback) override;
  void PrepareDiagnosticsAppBrowserContext(
      const base::FilePath& crx_path,
      const base::FilePath& swbn_path,
      PrepareDiagnosticsAppBrowserContextCallback callback) override {}
  bool IsChromeOSSystemExtensionProvider(
      const std::string& manufacturer) override;

  void set_is_chromeos_system_extension_provider(bool value) {
    is_chromeos_system_extension_provider_ = value;
  }

 private:
  bool is_chromeos_system_extension_provider_;
};

}  // namespace ash::shimless_rma

#endif  // ASH_WEBUI_SHIMLESS_RMA_BACKEND_FAKE_SHIMLESS_RMA_DELEGATE_H_
