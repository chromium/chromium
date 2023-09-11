// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SHIMLESS_RMA_CHROME_SHIMLESS_RMA_DELEGATE_H_
#define CHROME_BROWSER_ASH_SHIMLESS_RMA_CHROME_SHIMLESS_RMA_DELEGATE_H_

#include "ash/webui/shimless_rma/backend/shimless_rma_delegate.h"

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/shimless_rma/diagnostics_app_profile_helper.h"
#include "chrome/services/qrcode_generator/public/cpp/qrcode_generator_service.h"
#include "chrome/services/qrcode_generator/public/mojom/qrcode_generator.mojom.h"

namespace content {
class WebUI;
}  // namespace content

namespace ash::shimless_rma {

class ChromeShimlessRmaDelegate : public ShimlessRmaDelegate {
 public:
  explicit ChromeShimlessRmaDelegate(content::WebUI* web_ui);

  ChromeShimlessRmaDelegate(const ChromeShimlessRmaDelegate&) = delete;
  ChromeShimlessRmaDelegate& operator=(const ChromeShimlessRmaDelegate&) =
      delete;

  ~ChromeShimlessRmaDelegate() override;

  // ShimlessRmaDelegate:
  void ExitRmaThenRestartChrome() override;
  void ShowDiagnosticsDialog() override;
  void RefreshAccessibilityManagerProfile() override;
  void GenerateQrCode(const std::string& url,
                      base::OnceCallback<void(const std::string& qr_code_image)>
                          callback) override;
  void PrepareDiagnosticsAppBrowserContext(
      const base::FilePath& crx_path,
      const base::FilePath& swbn_path,
      PrepareDiagnosticsAppBrowserContextCallback callback) override;
  bool IsChromeOSSystemExtensionProvider(
      const std::string& manufacturer) override;

  void SetQRCodeServiceForTesting(
      base::RepeatingCallback<
          void(qrcode_generator::mojom::GenerateQRCodeRequestPtr request,
               qrcode_generator::QRImageGenerator::ResponseCallback callback)>
          qrcode_service_override);

  void SetDiagnosticsAppProfileHelperDelegateForTesting(
      DiagnosticsAppProfileHelperDelegate* delegate);

 private:
  void OnQrCodeGenerated(
      base::OnceCallback<void(const std::string& qr_code_image)> callback,
      const qrcode_generator::mojom::GenerateQRCodeResponsePtr response);

  // TODO(https://crbug.com/1431991): Remove this field once there is no
  // internal state (e.g. no `mojo::Remote`) that needs to be maintained by the
  // `QRImageGenerator` class.
  std::unique_ptr<qrcode_generator::QRImageGenerator> qrcode_service_;

  // Unit tests can set `qr_code_service_override_` to intercept QR code
  // requests and inject test-controlled QR code responses.
  //
  // Rationale for using a `RepeatingCallback` instead of implementing
  // dependency injection using virtual methods: 1) `GenerateQRCode` will become
  // a free function after shipping https://crbug.com/1431991, 2) in general
  // `RepeatingCallback` is equivalent to a pure interface with a single method,
  // (note that `RepeatingCallback` below has the same signature as
  // `GenerateQRCode`).
  base::RepeatingCallback<void(
      qrcode_generator::mojom::GenerateQRCodeRequestPtr request,
      qrcode_generator::QRImageGenerator::ResponseCallback callback)>
      qrcode_service_override_;

  DiagnosticsAppProfileHelperDelegate diagnostics_app_profile_helper_delegete_;
  raw_ptr<DiagnosticsAppProfileHelperDelegate>
      diagnostics_app_profile_helper_delegete_ptr_{
          &diagnostics_app_profile_helper_delegete_};

  base::WeakPtrFactory<ChromeShimlessRmaDelegate> weak_ptr_factory_{this};
};

}  // namespace ash::shimless_rma

#endif  // CHROME_BROWSER_ASH_SHIMLESS_RMA_CHROME_SHIMLESS_RMA_DELEGATE_H_
