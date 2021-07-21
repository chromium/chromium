// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_CONTINUE_TASK_VIEW_H_
#define ASH_APP_LIST_VIEWS_CONTINUE_TASK_VIEW_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ui/views/view.h"

namespace views {
class Label;
}

namespace ash {
class SearchResult;

// A view with a suggested task for the "Continue" section.
class ASH_EXPORT ContinueTaskView : public views::View {
 public:
  METADATA_HEADER(ContinueTaskView);

  explicit ContinueTaskView(SearchResult* result);
  ContinueTaskView(const ContinueTaskView&) = delete;
  ContinueTaskView& operator=(const ContinueTaskView&) = delete;
  ~ContinueTaskView() override;

  // views::View:
  void OnThemeChanged() override;

  SearchResult* result() { return result_; }

 private:
  void SetText(const std::u16string& task_title);

  SearchResult* result_;
  views::Label* title_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_CONTINUE_TASK_VIEW_H_
