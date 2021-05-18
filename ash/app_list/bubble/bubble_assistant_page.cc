// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/bubble/bubble_assistant_page.h"

#include <memory>
#include <utility>

#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

using views::BoxLayout;

namespace ash {

BubbleAssistantPage::BubbleAssistantPage() {
  SetLayoutManager(
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical));

  // TODO(https://crbug.com/1204551): Sort out whether this view needs its own
  // search box, or if it can use the one owned by the parent. The assistant
  // page in the tablet launcher owns its own search box, but that may be a side
  // effect of animations or the historical need to host assistant in a widget.

  // TODO(https://crbug.com/1204551): Embed the assistant.
  AddChildView(std::make_unique<views::Label>(u"Assistant"));
}

BubbleAssistantPage::~BubbleAssistantPage() = default;

}  // namespace ash
