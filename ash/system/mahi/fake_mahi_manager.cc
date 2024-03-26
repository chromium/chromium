// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/fake_mahi_manager.h"

#include <utility>
#include <vector>

#include "ash/system/mahi/mahi_constants.h"
#include "ash/system/mahi/mahi_panel_widget.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "ui/display/screen.h"

namespace ash {

namespace {

constexpr char16_t kDefaultAnswer[] = u"Fake answer";

constexpr char16_t kDefaultContentTitle[] = u"fake content title";

const std::vector<chromeos::MahiOutline> kDefaultOutlines(
    {chromeos::MahiOutline(/*id=*/1, u"Outline 1"),
     chromeos::MahiOutline(/*id=*/2, u"Outline 2"),
     chromeos::MahiOutline(/*id=*/3, u"Outline 3"),
     chromeos::MahiOutline(/*id=*/4, u"Outline 4"),
     chromeos::MahiOutline(/*id=*/5, u"Outline 5")});

constexpr char16_t kDefaultSummaryText[] =
    u"fake summary text\nfake summary text\nfake summary text\nfake summary "
    u"text\nfake summary text";

}  // namespace

FakeMahiManager::FakeMahiManager() = default;

FakeMahiManager::~FakeMahiManager() = default;

void FakeMahiManager::OpenMahiPanel(int64_t display_id) {
  mahi_panel_widget_ = MahiPanelWidget::CreatePanelWidget(display_id);
  mahi_panel_widget_->Show();
}

std::u16string FakeMahiManager::GetContentTitle() {
  return content_title_.value_or(kDefaultContentTitle);
}

gfx::ImageSkia FakeMahiManager::GetContentIcon() {
  return content_icon_;
}

void FakeMahiManager::GetSummary(MahiSummaryCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     summary_text_.value_or(kDefaultSummaryText),
                     chromeos::MahiResponseStatus::kSuccess),
      base::Seconds(mahi_constants::kFakeMahiManagerLoadSummaryDelaySeconds));
}

void FakeMahiManager::GetOutlines(MahiOutlinesCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), kDefaultOutlines,
                     chromeos::MahiResponseStatus::kSuccess),
      base::Seconds(mahi_constants::kFakeMahiManagerLoadOutlinesDelaySeconds));
}

void FakeMahiManager::AnswerQuestion(const std::u16string& question,
                                     bool current_panel_content,
                                     MahiAnswerQuestionCallback callback) {
  asked_question_ = question;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), answer_text_.value_or(kDefaultAnswer),
                     chromeos::MahiResponseStatus::kSuccess),
      base::Seconds(mahi_constants::kFakeMahiManagerLoadAnswerDelaySeconds));
}

void FakeMahiManager::OnContextMenuClicked(
    crosapi::mojom::MahiContextMenuRequestPtr context_menu_request) {
  OpenMahiPanel(display::Screen::GetScreen()->GetPrimaryDisplay().id());
}

}  // namespace ash
