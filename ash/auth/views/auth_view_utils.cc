// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/auth/views/auth_view_utils.h"

#include "ash/login/ui/non_accessible_view.h"
#include "ui/views/view.h"

namespace ash {

views::View* AddVerticalSpace(views::View* view, int height) {
  auto* spacer = view->AddChildView(std::make_unique<NonAccessibleView>());
  spacer->SetPreferredSize(gfx::Size(1, height));
  return spacer;
}

views::View* AddHorizontalSpace(views::View* view, int width) {
  auto* spacer = view->AddChildView(std::make_unique<NonAccessibleView>());
  spacer->SetPreferredSize(gfx::Size(width, 1));
  return spacer;
}
}  // namespace ash
