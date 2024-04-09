// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_FAKE_MAHI_MANAGER_H_
#define ASH_SYSTEM_MAHI_FAKE_MAHI_MANAGER_H_

#include <optional>
#include <string>

#include "ash/ash_export.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "chromeos/crosapi/mojom/mahi.mojom.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace ash {

// A fake implementation of `MahiManager` used for development only. Returns
// predetermined contents asyncly. Created only when
// `chromeos::switches::kUseFakeMahiManager` is enabled.
class ASH_EXPORT FakeMahiManager : public chromeos::MahiManager {
 public:
  FakeMahiManager();
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
                      MahiAnswerQuestionCallback callback) override;
  void GetSuggestedQuestion(
      MahiGetSuggestedQuestionCallback callback) override {}
  void SetCurrentFocusedPageInfo(
      crosapi::mojom::MahiPageInfoPtr info) override {}
  void OnContextMenuClicked(
      crosapi::mojom::MahiContextMenuRequestPtr context_menu_request) override;
  void OpenFeedbackDialog() override {}
  bool IsEnabled() override;

  void set_answer_text(const std::u16string& answer_text) {
    answer_text_ = answer_text;
  }

  void set_content_icon(const gfx::ImageSkia& content_icon) {
    content_icon_ = content_icon;
  }

  void set_content_title(const std::u16string& content_title) {
    content_title_ = content_title;
  }

  void set_summary_text(const std::u16string& summary_text) {
    summary_text_ = summary_text;
  }

 private:
  std::optional<std::u16string> answer_text_;
  std::optional<std::u16string> asked_question_;
  gfx::ImageSkia content_icon_;
  std::optional<std::u16string> content_title_;
  std::optional<std::u16string> summary_text_;

  // The widget contains the Mahi main panel.
  views::UniqueWidgetPtr mahi_panel_widget_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MAHI_FAKE_MAHI_MANAGER_H_
