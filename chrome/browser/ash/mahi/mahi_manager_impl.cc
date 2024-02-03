// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mahi/mahi_manager_impl.h"

#include <stdint.h>

#include <algorithm>

#include "ash/system/mahi/mahi_panel_widget.h"
#include "base/functional/callback.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace ash {

MahiManagerImpl::MahiManagerImpl() = default;

MahiManagerImpl::~MahiManagerImpl() {
  mahi_panel_widget_.reset();
}

void MahiManagerImpl::OpenMahiPanel(int64_t display_id) {
  mahi_panel_widget_ = MahiPanelWidget::CreatePanelWidget(display_id);
  mahi_panel_widget_->Show();
}

void MahiManagerImpl::GetSummary(MahiSummaryCallback callback) {
  std::move(callback).Run(u"summary text");
}

}  // namespace ash
