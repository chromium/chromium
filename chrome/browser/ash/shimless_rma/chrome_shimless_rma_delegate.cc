// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/shimless_rma/chrome_shimless_rma_delegate.h"

#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/login/chrome_restart_request.h"
#include "chrome/browser/ash/shimless_rma/diagnostics_app_profile_helper.h"
#include "chrome/browser/ash/system/device_disabling_manager.h"
#include "chrome/browser/ui/webui/ash/diagnostics_dialog.h"
#include "chrome/services/qrcode_generator/public/cpp/qrcode_generator_service.h"
#include "chrome/services/qrcode_generator/public/mojom/qrcode_generator.mojom.h"
#include "content/public/browser/web_ui.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {
namespace shimless_rma {

ChromeShimlessRmaDelegate::ChromeShimlessRmaDelegate(content::WebUI* web_ui) {}
ChromeShimlessRmaDelegate::~ChromeShimlessRmaDelegate() = default;

void ChromeShimlessRmaDelegate::ExitRmaThenRestartChrome() {
  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();
  base::CommandLine command_line(browser_command_line);
  command_line.AppendSwitch(::ash::switches::kRmaNotAllowed);
  // Remove any attempts to launch RMA.
  command_line.RemoveSwitch(::ash::switches::kLaunchRma);
  ash::RestartChrome(command_line, ash::RestartChromeReason::kUserless);
}

void ChromeShimlessRmaDelegate::ShowDiagnosticsDialog() {
  // Don't launch Diagnostics if device is disabled.
  if (system::DeviceDisablingManager::IsDeviceDisabledDuringNormalOperation()) {
    return;
  }

  DiagnosticsDialog::ShowDialog();
}

void ChromeShimlessRmaDelegate::RefreshAccessibilityManagerProfile() {
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  DCHECK(accessibility_manager);
  accessibility_manager->OnShimlessRmaLaunched();
}

void ChromeShimlessRmaDelegate::GenerateQrCode(
    const std::string& url,
    base::OnceCallback<void(const std::string& qr_code_image)> callback) {
  qrcode_generator::mojom::GenerateQRCodeRequestPtr request =
      qrcode_generator::mojom::GenerateQRCodeRequest::New();
  request->data = url;
  request->center_image = qrcode_generator::mojom::CenterImage::CHROME_DINO;

  auto qrcode_service_callback =
      base::BindOnce(&ChromeShimlessRmaDelegate::OnQrCodeGenerated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));
  if (qrcode_service_override_.is_null()) {
    if (!qrcode_service_) {
      qrcode_service_ = std::make_unique<qrcode_generator::QRImageGenerator>();
    }
    qrcode_service_->GenerateQRCode(std::move(request),
                                    std::move(qrcode_service_callback));
  } else {
    qrcode_service_override_.Run(std::move(request),
                                 std::move(qrcode_service_callback));
  }
}

void ChromeShimlessRmaDelegate::OnQrCodeGenerated(
    base::OnceCallback<void(const std::string& qr_code_image)> callback,
    const qrcode_generator::mojom::GenerateQRCodeResponsePtr response) {
  DCHECK(response->error_code ==
         qrcode_generator::mojom::QRCodeGeneratorError::NONE);

  std::vector<unsigned char> bytes;
  gfx::PNGCodec::EncodeBGRASkBitmap(response->bitmap, false, &bytes);
  std::move(callback).Run(std::string(bytes.begin(), bytes.end()));
}

void ChromeShimlessRmaDelegate::SetQRCodeServiceForTesting(
    base::RepeatingCallback<
        void(qrcode_generator::mojom::GenerateQRCodeRequestPtr request,
             qrcode_generator::QRImageGenerator::ResponseCallback callback)>
        qrcode_service_override) {
  qrcode_service_override_ = std::move(qrcode_service_override);
}

void ChromeShimlessRmaDelegate::PrepareDiagnosticsAppBrowserContext(
    const base::FilePath& crx_path,
    const base::FilePath& swbn_path,
    PrepareDiagnosticsAppBrowserContextCallback callback) {
  CHECK(::ash::features::IsShimlessRMA3pDiagnosticsEnabled());
  PrepareDiagnosticsAppProfile(diagnostics_app_profile_helper_delegete_ptr_,
                               crx_path, swbn_path, std::move(callback));
}

void ChromeShimlessRmaDelegate::
    SetDiagnosticsAppProfileHelperDelegateForTesting(
        DiagnosticsAppProfileHelperDelegate* delegate) {
  diagnostics_app_profile_helper_delegete_ptr_ = delegate;
}

}  // namespace shimless_rma
}  // namespace ash
