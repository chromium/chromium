// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/fake_mahi_manager.h"

#include <utility>
#include <vector>

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/system/mahi/mahi_constants.h"
#include "ash/system/mahi/mahi_panel_widget.h"
#include "ash/system/mahi/mahi_ui_controller.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "ui/display/screen.h"
#include "url/gurl.h"

namespace ash {

namespace {

// If true, callbacks passed to `FakeMahiManager` are handled asyncly with zero
// time duration.
bool g_use_zero_duration = false;

// Constants -------------------------------------------------------------------

constexpr char16_t kDefaultAnswer[] = u"Fake answer";

constexpr char16_t kDefaultContentTitle[] = u"fake content title";

constexpr char kDefaultContentUrl[] = "https://en.wikipedia.org/wiki/Wombat";

const std::vector<chromeos::MahiOutline> kDefaultOutlines(
    {chromeos::MahiOutline(/*id=*/1, u"Outline 1"),
     chromeos::MahiOutline(/*id=*/2, u"Outline 2"),
     chromeos::MahiOutline(/*id=*/3, u"Outline 3"),
     chromeos::MahiOutline(/*id=*/4, u"Outline 4"),
     chromeos::MahiOutline(/*id=*/5, u"Outline 5")});

constexpr char16_t kDefaultContentText[] =
    u"fake content text\nfake content text\nfake content text\nfake content "
    u"text\nfake content text";

constexpr char16_t kDefaultSummaryText[] =
    u"fake summary text\nfake summary text\nfake summary text\nfake summary "
    u"text\nfake summary text";

constexpr char kMahiSettingsUrl[] =
    "chrome://os-settings/systemPreferences?settingId=612";

}  // namespace

using crosapi::mojom::MahiContextMenuActionType;

FakeMahiManager::FakeMahiManager() = default;

FakeMahiManager::~FakeMahiManager() = default;

std::u16string FakeMahiManager::GetContentTitle() {
  return content_title_.value_or(kDefaultContentTitle);
}

gfx::ImageSkia FakeMahiManager::GetContentIcon() {
  return content_icon_;
}

GURL FakeMahiManager::GetContentUrl() {
  return GURL(kDefaultContentUrl);
}

void FakeMahiManager::GetContent(MahiContentCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     summary_text_.value_or(kDefaultContentText),
                     chromeos::MahiGetContentResponseStatus::kSuccess),
      g_use_zero_duration
          ? base::TimeDelta()
          : base::Seconds(
                mahi_constants::kFakeMahiManagerGetContentDelaySeconds));
}

void FakeMahiManager::GetSummary(MahiSummaryCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     summary_text_.value_or(kDefaultSummaryText),
                     chromeos::MahiResponseStatus::kSuccess),
      g_use_zero_duration
          ? base::TimeDelta()
          : base::Seconds(
                mahi_constants::kFakeMahiManagerLoadSummaryDelaySeconds));
}

void FakeMahiManager::GetOutlines(MahiOutlinesCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), kDefaultOutlines,
                     chromeos::MahiResponseStatus::kSuccess),
      g_use_zero_duration
          ? base::TimeDelta()
          : base::Seconds(
                mahi_constants::kFakeMahiManagerLoadOutlinesDelaySeconds));
}

void FakeMahiManager::AnswerQuestion(const std::u16string& question,
                                     bool current_panel_content,
                                     MahiAnswerQuestionCallback callback) {
  asked_question_ = question;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), answer_text_.value_or(kDefaultAnswer),
                     chromeos::MahiResponseStatus::kSuccess),
      g_use_zero_duration
          ? base::TimeDelta()
          : base::Seconds(
                mahi_constants::kFakeMahiManagerLoadAnswerDelaySeconds));
}

void FakeMahiManager::OnContextMenuClicked(
    crosapi::mojom::MahiContextMenuRequestPtr context_menu_request) {
  switch (context_menu_request->action_type) {
    case MahiContextMenuActionType::kSummary:
    case MahiContextMenuActionType::kOutline:
      // TODO(b/318565610): Update the behaviour of kOutline.
      OpenMahiPanel(
          context_menu_request->display_id,
          context_menu_request->mahi_menu_bounds.value_or(gfx::Rect()));

      return;
    case MahiContextMenuActionType::kQA:
      OpenMahiPanel(context_menu_request->display_id,
                    context_menu_request->mahi_menu_bounds.has_value()
                        ? context_menu_request->mahi_menu_bounds.value()
                        : gfx::Rect());

      // Ask question.
      if (!context_menu_request->question) {
        return;
      }

      // Because we call `MahiUiController::SendQuestion` right after
      // opening the panel here, `SendQuestion` will cancel the call to get
      // summary due to `MahiUiController::InvalidatePendingRequests()`. Thus,
      // we need to update the summary after answering the question to make sure
      // that user gets summary when navigating back to the summary UI
      // (b/345621992).
      ui_controller_.SendQuestion(
          context_menu_request->question.value(),
          /*current_panel_content=*/true,
          MahiUiController::QuestionSource::kMenuView,
          /*update_summary_after_answer_question=*/true);
      return;
    case MahiContextMenuActionType::kSettings:
      NewWindowDelegate::GetInstance()->OpenUrl(
          GURL(kMahiSettingsUrl),
          NewWindowDelegate::OpenUrlFrom::kUserInteraction,
          NewWindowDelegate::Disposition::kNewForegroundTab);
      return;
    case MahiContextMenuActionType::kNone:
      return;
  }
}

void FakeMahiManager::OpenMahiPanel(int64_t display_id,
                                    const gfx::Rect& mahi_menu_bounds) {
  ui_controller_.OpenMahiPanel(display_id, mahi_menu_bounds);
}

bool FakeMahiManager::IsEnabled() {
  return mahi_enabled_;
}

bool FakeMahiManager::AllowRepeatingAnswers() {
  return false;
}

void FakeMahiManager::AnswerQuestionRepeating(
    const std::u16string& question,
    bool current_panel_content,
    MahiAnswerQuestionCallbackRepeating callback) {}

void FakeMahiManager::SetMediaAppPDFFocused() {}

// ScopedFakeMahiManagerZeroDuration -------------------------------------------

ScopedFakeMahiManagerZeroDuration::ScopedFakeMahiManagerZeroDuration() {
  CHECK(!g_use_zero_duration);
  g_use_zero_duration = true;
}

ScopedFakeMahiManagerZeroDuration::~ScopedFakeMahiManagerZeroDuration() {
  CHECK(g_use_zero_duration);
  g_use_zero_duration = false;
}

}  // namespace ash
