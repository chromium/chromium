// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/continue_task_view.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/bubble/bubble_utils.h"
#include "base/strings/string_util.h"
#include "extensions/common/constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

ContinueTaskView::ContinueTaskView(const std::u16string& task_title) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  auto* label = AddChildView(std::make_unique<views::Label>(task_title));
  bubble_utils::ApplyStyle(label, bubble_utils::LabelStyle::kBody);
}

ContinueTaskView::~ContinueTaskView() = default;

BEGIN_METADATA(ContinueTaskView, views::View)
END_METADATA

}  // namespace ash
