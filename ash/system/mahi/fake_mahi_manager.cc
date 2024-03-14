// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/fake_mahi_manager.h"

#include <algorithm>

#include "ash/system/mahi/mahi_constants.h"
#include "ash/system/mahi/mahi_panel_widget.h"
#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "ui/display/screen.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

FakeMahiManager::FakeMahiManager(bool enable_callback_delays_for_animations)
    : content_title_(u"fake content title"),
      summary_text_(
          u"fake summary text\nfake summary text\nfake summary text\nfake "
          u"summary text\nfake summary text"),
      enable_fake_delays_for_animations_(
          enable_callback_delays_for_animations) {}

FakeMahiManager::~FakeMahiManager() {
  mahi_panel_widget_.reset();
}

void FakeMahiManager::OpenMahiPanel(int64_t display_id) {
  mahi_panel_widget_ = MahiPanelWidget::CreatePanelWidget(display_id);
  mahi_panel_widget_->Show();
}

std::u16string FakeMahiManager::GetContentTitle() {
  return content_title_;
}

gfx::ImageSkia FakeMahiManager::GetContentIcon() {
  return content_icon_;
}

void FakeMahiManager::GetSummary(MahiSummaryCallback callback) {
  if (!enable_fake_delays_for_animations_) {
    std::move(callback).Run(summary_text_,
                            chromeos::MahiResponseStatus::kSuccess);
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), summary_text_,
                     chromeos::MahiResponseStatus::kSuccess),
      base::Seconds(mahi_constants::kFakeMahiManagerLoadSummaryDelaySeconds));
}

void FakeMahiManager::GetOutlines(MahiOutlinesCallback callback) {
  std::vector<chromeos::MahiOutline> outlines;
  for (int i = 0; i < 5; i++) {
    outlines.emplace_back(
        chromeos::MahiOutline(i, u"Outline " + base::NumberToString16(i)));
  }

  if (!enable_fake_delays_for_animations_) {
    std::move(callback).Run(outlines, chromeos::MahiResponseStatus::kSuccess);
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), outlines,
                     chromeos::MahiResponseStatus::kSuccess),
      base::Seconds(mahi_constants::kFakeMahiManagerLoadOutlinesDelaySeconds));
}

void FakeMahiManager::OnContextMenuClicked(
    crosapi::mojom::MahiContextMenuRequestPtr context_menu_request) {
  OpenMahiPanel(display::Screen::GetScreen()->GetPrimaryDisplay().id());
}

void FakeMahiManager::OpenFeedbackDialog() {
  open_feedback_dialog_called_count_++;
}

}  // namespace ash
