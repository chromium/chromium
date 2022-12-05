// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/shimless_rma/chrome_shimless_rma_delegate.h"

#include <string>
#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/login/chrome_restart_request.h"
#include "chrome/browser/ash/system/device_disabling_manager.h"
#include "chrome/browser/ui/webui/ash/diagnostics_dialog.h"
#include "chrome/services/qrcode_generator/public/cpp/qrcode_generator_service.h"
#include "chrome/services/qrcode_generator/public/mojom/qrcode_generator.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {
namespace shimless_rma {

ChromeShimlessRmaDelegate::ChromeShimlessRmaDelegate() = default;
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
  request->should_render = true;
  request->center_image = qrcode_generator::mojom::CenterImage::CHROME_DINO;

  if (!qrcode_service_remote_) {
    qrcode_service_remote_ = qrcode_generator::LaunchQRCodeGeneratorService();
  }
  qrcode_generator::mojom::QRCodeGeneratorService* qrcode_service =
      qrcode_service_remote_.get();

  qrcode_service->GenerateQRCode(
      std::move(request),
      base::BindOnce(&ChromeShimlessRmaDelegate::OnQrCodeGenerated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
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
    mojo::Remote<qrcode_generator::mojom::QRCodeGeneratorService>&& remote) {
  qrcode_service_remote_ = std::move(remote);
}

}  // namespace shimless_rma
}  // namespace ash
