// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mahi/mahi_manager_ash.h"

#include <stdint.h>

#include <algorithm>

#include "ash/system/mahi/mahi_panel_widget.h"
#include "base/functional/callback.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace ash {

MahiManagerAsh::MahiManagerAsh() = default;

MahiManagerAsh::~MahiManagerAsh() {
  mahi_panel_widget_.reset();
}

void MahiManagerAsh::OpenMahiPanel(int64_t display_id) {
  mahi_panel_widget_ = MahiPanelWidget::CreatePanelWidget(display_id);
  mahi_panel_widget_->Show();
}

void MahiManagerAsh::GetSummary(MahiSummaryCallback callback) {
  std::move(callback).Run(u"summary text");
}

}  // namespace ash
