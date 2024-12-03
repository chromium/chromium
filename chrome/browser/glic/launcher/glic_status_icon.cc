// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/launcher/glic_status_icon.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/launcher/glic_controller.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "glic_status_icon.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"

GlicStatusIcon::GlicStatusIcon(GlicController* controller,
                               StatusTray* status_tray)
    : controller_(controller), status_tray_(status_tray) {
  // TODO(https://crbug.com/378139555): Use correct icon.
  gfx::ImageSkia status_tray_icon = gfx::CreateVectorIcon(
      vector_icons::kProductRefreshIcon, gfx::kPlaceholderColor);

  status_icon_ = status_tray_->CreateStatusIcon(
      StatusTray::GLIC_ICON, status_tray_icon,
      l10n_util::GetStringUTF16(IDS_GLIC_STATUS_ICON_TOOLTIP));
  status_icon_->AddObserver(this);

  // TODO(https://crbug.com/378140640): Add context menu.
}

GlicStatusIcon::~GlicStatusIcon() {
  status_icon_->RemoveObserver(this);
  status_tray_->RemoveStatusIcon(status_icon_);
}

void GlicStatusIcon::OnStatusIconClicked() {
  controller_->Show();
}
