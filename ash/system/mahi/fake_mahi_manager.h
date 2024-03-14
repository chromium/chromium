// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_FAKE_MAHI_MANAGER_H_
#define ASH_SYSTEM_MAHI_FAKE_MAHI_MANAGER_H_

#include <string>

#include "ash/ash_export.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace ash {

// A fake implementation of `MahiManager`.
class ASH_EXPORT FakeMahiManager : public chromeos::MahiManager {
 public:
  explicit FakeMahiManager(bool enable_callback_delays_for_animations = false);
  FakeMahiManager(const FakeMahiManager&) = delete;
  FakeMahiManager& operator=(const FakeMahiManager&) = delete;
  ~FakeMahiManager() override;

  // MahiManager:
  void OpenMahiPanel(int64_t display_id) override;
  std::u16string GetContentTitle() override;
  gfx::ImageSkia GetContentIcon() override;
  void GetSummary(MahiSummaryCallback callback) override;
  void GetOutlines(MahiOutlinesCallback callback) override;
  void GoToOutlineContent(int outline_id) override {}
  void AnswerQuestion(const std::u16string& question,
                      bool current_panel_content,
                      MahiAnswerQuestionCallback callback) override {}
  void GetSuggestedQuestion(
      MahiGetSuggestedQuestionCallback callback) override {}
  void SetCurrentFocusedPageInfo(
      crosapi::mojom::MahiPageInfoPtr info) override {}
  void OnContextMenuClicked(
      crosapi::mojom::MahiContextMenuRequestPtr context_menu_request) override;
  void OpenFeedbackDialog() override;

  void set_content_title(const std::u16string& content_title) {
    content_title_ = content_title;
  }

  void set_content_icon(const gfx::ImageSkia& content_icon) {
    content_icon_ = content_icon;
  }

  void set_summary_text(const std::u16string& summary_text) {
    summary_text_ = summary_text;
  }

  int open_feedback_dialog_called_count() {
    return open_feedback_dialog_called_count_;
  }

  void set_enable_fake_delays_for_animations(
      bool enable_fake_delays_for_animations) {
    enable_fake_delays_for_animations_ = enable_fake_delays_for_animations;
  }

 private:
  std::u16string content_title_;
  gfx::ImageSkia content_icon_;
  std::u16string summary_text_;

  bool enable_fake_delays_for_animations_ = false;

  int open_feedback_dialog_called_count_ = 0;

  // The widget contains the Mahi main panel.
  views::UniqueWidgetPtr mahi_panel_widget_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MAHI_FAKE_MAHI_MANAGER_H_
