// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_UI_CANDIDATE_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_UI_CANDIDATE_VIEW_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "ui/base/ime/candidate_window.h"
#include "ui/chromeos/ui_chromeos_export.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

namespace ui {
namespace ime {

// CandidateView renderes a row of a candidate.
class UI_CHROMEOS_EXPORT CandidateView : public views::Button {
 public:
  CandidateView(views::ButtonListener* listener,
                ui::CandidateWindow::Orientation orientation);
  ~CandidateView() override {}

  void GetPreferredWidths(int* shortcut_width, int* candidate_width);

  void SetWidths(int shortcut_width, int candidate_width);

  void SetEntry(const ui::CandidateWindow::Entry& entry);

  // Sets infolist icon.
  void SetInfolistIcon(bool enable);

  void SetHighlighted(bool highlighted);

  void SetPositionData(int index, int total);

 private:
  friend class CandidateWindowViewTest;
  FRIEND_TEST_ALL_PREFIXES(CandidateWindowViewTest, ShortcutSettingTest);

  // Overridden from views::Button:
  void StateChanged(ButtonState old_state) override;

  // Overridden from View:
  const char* GetClassName() const override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // The orientation of the candidate view.
  ui::CandidateWindow::Orientation orientation_;

  // Views created in the class will be part of tree of |this|, so these
  // child views will be deleted when |this| is deleted.

  // The shortcut label renders shortcut numbers like 1, 2, and 3.
  views::Label* shortcut_label_ = nullptr;
  // The candidate label renders candidates.
  views::Label* candidate_label_ = nullptr;
  // The annotation label renders annotations.
  views::Label* annotation_label_ = nullptr;
  // The infolist icon.
  views::View* infolist_icon_ = nullptr;

  int shortcut_width_ = 0;
  int candidate_width_ = 0;
  bool highlighted_ = false;

  // 0-based index of this candidate e.g. [0, total_candidates_ -1].
  int candidate_index_;
  int total_candidates_;

  DISALLOW_COPY_AND_ASSIGN(CandidateView);
};

}  // namespace ime
}  // namespace ui

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_UI_CANDIDATE_VIEW_H_
