// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/fake_mahi_manager.h"

#include <algorithm>

#include "ash/system/mahi/mahi_panel_widget.h"
#include "base/functional/callback.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

FakeMahiManager::FakeMahiManager() {
  set_summary_text(u"This is a fake summary text");
}

FakeMahiManager::~FakeMahiManager() {
  mahi_panel_widget_.reset();
}

void FakeMahiManager::OpenMahiPanel(int64_t display_id) {
  mahi_panel_widget_ = MahiPanelWidget::CreatePanelWidget(display_id);
  mahi_panel_widget_->Show();
}

std::u16string FakeMahiManager::GetContentTitle() {
  return u"fake content title";
}

gfx::ImageSkia FakeMahiManager::GetContentIcon() {
  return gfx::ImageSkia();
}

void FakeMahiManager::GetSummary(MahiSummaryCallback callback) {
  std::move(callback).Run(summary_text_);
}

}  // namespace ash
