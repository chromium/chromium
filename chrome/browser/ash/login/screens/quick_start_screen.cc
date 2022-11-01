// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/quick_start_screen.h"
#include <memory>

#include "base/i18n/time_formatting.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/oobe_quick_start/target_device_bootstrap_controller.h"
#include "chrome/browser/ash/login/oobe_quick_start/verification_shapes.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/quick_start_screen_handler.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace ash {

// static
std::string QuickStartScreen::GetResultString(Result result) {
  switch (result) {
    case Result::CANCEL:
      return "Cancel";
  }
}

QuickStartScreen::QuickStartScreen(base::WeakPtr<TView> view,
                                   const ScreenExitCallback& exit_callback)
    : BaseScreen(QuickStartView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

QuickStartScreen::~QuickStartScreen() {
  UnbindFromBootstrapController();
}

bool QuickStartScreen::MaybeSkip(WizardContext& context) {
  return false;
}

void QuickStartScreen::ShowImpl() {
  if (!view_)
    return;

  view_->Show();
  bootstrap_controller_ =
      LoginDisplayHost::default_host()->GetQuickStartBootstrapController();
  bootstrap_controller_->AddObserver(this);
  bootstrap_controller_->StartAdvertising();
}

void QuickStartScreen::HideImpl() {
  if (!bootstrap_controller_)
    return;
  bootstrap_controller_->StopAdvertising();
  UnbindFromBootstrapController();
}

void QuickStartScreen::OnUserAction(const base::Value::List& args) {}

void QuickStartScreen::OnStatusChanged(
    const quick_start::TargetDeviceBootstrapController::Status& status) {
  using Step = quick_start::TargetDeviceBootstrapController::Step;
  using QRCodePixelData =
      quick_start::TargetDeviceBootstrapController::QRCodePixelData;

  switch (status.step) {
    case Step::QR_CODE_VERIFICATION: {
      CHECK(absl::holds_alternative<QRCodePixelData>(status.payload));
      if (!view_)
        return;
      const auto& code = absl::get<QRCodePixelData>(status.payload);
      base::Value::List qr_code_list;
      for (const auto& it : code) {
        qr_code_list.Append(base::Value(static_cast<bool>(it & 1)));
      }
      view_->SetQRCode(std::move(qr_code_list));
      return;
    }
    case Step::NONE:
    case Step::ERROR:
    case Step::ADVERTISING:
    case Step::CONNECTED:
      NOTIMPLEMENTED();
  }
}

void QuickStartScreen::UnbindFromBootstrapController() {
  if (!bootstrap_controller_)
    return;
  bootstrap_controller_->RemoveObserver(this);
  bootstrap_controller_.reset();
}

void QuickStartScreen::SendRandomFiguresForTesting() const {
  if (!view_)
    return;

  std::string token = base::UTF16ToASCII(
      base::TimeFormatWithPattern(base::Time::Now(), "MMMMdjmmss"));
  const auto& shapes = quick_start::GenerateShapes(token);
  view_->SetShapes(shapes);
}

}  // namespace ash
