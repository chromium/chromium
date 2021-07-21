// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/continue_task_view.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "ash/app_list/model/search/search_result.h"
#include "ash/bubble/bubble_utils.h"
#include "base/strings/string_util.h"
#include "extensions/common/constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

ContinueTaskView::ContinueTaskView(SearchResult* task_result)
    : result_(task_result) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  title_ = AddChildView(std::make_unique<views::Label>());
  SetText(result()->title());
  title_->SetAccessibleName(result()->accessible_name());
}

ContinueTaskView::~ContinueTaskView() = default;

void ContinueTaskView::SetText(const std::u16string& text) {
  title_->SetText(text);
  PreferredSizeChanged();
}

void ContinueTaskView::OnThemeChanged() {
  views::View::OnThemeChanged();
  bubble_utils::ApplyStyle(title_, bubble_utils::LabelStyle::kBody);
}

BEGIN_METADATA(ContinueTaskView, views::View)
END_METADATA

}  // namespace ash
