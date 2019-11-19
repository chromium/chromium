// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/palette/tools/capture_region_mode.h"

#include "ash/public/cpp/toast_data.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/palette/palette_ids.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/utility/screenshot_controller.h"
#include "base/bind.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

const char kToastId[] = "palette_capture_region";
const int kToastDurationMs = 2500;

}  // namespace

CaptureRegionMode::CaptureRegionMode(Delegate* delegate)
    : CommonPaletteTool(delegate) {}

CaptureRegionMode::~CaptureRegionMode() = default;

PaletteGroup CaptureRegionMode::GetGroup() const {
  return PaletteGroup::MODE;
}

PaletteToolId CaptureRegionMode::GetToolId() const {
  return PaletteToolId::CAPTURE_REGION;
}

const gfx::VectorIcon& CaptureRegionMode::GetActiveTrayIcon() const {
  return kPaletteTrayIconCaptureRegionIcon;
}

void CaptureRegionMode::OnEnable() {
  CommonPaletteTool::OnEnable();

  ToastData toast(kToastId, l10n_util::GetStringUTF16(
                                IDS_ASH_STYLUS_TOOLS_CAPTURE_REGION_TOAST),
                  kToastDurationMs, base::Optional<base::string16>());
  Shell::Get()->toast_manager()->Show(toast);

  auto* screenshot_controller = Shell::Get()->screenshot_controller();
  screenshot_controller->set_pen_events_only(true);
  screenshot_controller->StartPartialScreenshotSession(
      false /* draw_overlay_immediately */);
  screenshot_controller->set_on_screenshot_session_done(base::BindOnce(
      &CaptureRegionMode::OnScreenshotDone, weak_factory_.GetWeakPtr()));

  delegate()->HidePalette();
}

void CaptureRegionMode::OnDisable() {
  CommonPaletteTool::OnDisable();

  // If the user manually cancelled the action we need to make sure to cancel
  // the screenshot session as well.
  Shell::Get()->screenshot_controller()->CancelScreenshotSession();
}

views::View* CaptureRegionMode::CreateView() {
  return CreateDefaultView(
      l10n_util::GetStringUTF16(IDS_ASH_STYLUS_TOOLS_CAPTURE_REGION_ACTION));
}

const gfx::VectorIcon& CaptureRegionMode::GetPaletteIcon() const {
  return kPaletteActionCaptureRegionIcon;
}

void CaptureRegionMode::OnScreenshotDone() {
  // The screenshot finished, so disable the tool.
  delegate()->DisableTool(GetToolId());
}

}  // namespace ash
