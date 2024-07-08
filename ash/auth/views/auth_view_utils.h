// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AUTH_VIEWS_AUTH_VIEW_UTILS_H_
#define ASH_AUTH_VIEWS_AUTH_VIEW_UTILS_H_

#include "ui/views/view.h"

namespace ash {

views::View* AddVerticalSpace(views::View* view, int height);

views::View* AddHorizontalSpace(views::View* view, int width);

}  // namespace ash

#endif  // ASH_AUTH_VIEWS_AUTH_VIEW_UTILS_H_
