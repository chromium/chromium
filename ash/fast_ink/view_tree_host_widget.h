// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FAST_INK_VIEW_TREE_HOST_WIDGET_H_
#define ASH_FAST_INK_VIEW_TREE_HOST_WIDGET_H_

#include "ui/views/widget/widget.h"

namespace ash {

// Creates a Widget that creates a CompositorFrame from a view tree within the
// widget and submits it directly w/o going through cc.
views::Widget* CreateViewTreeHostWidget(views::Widget::InitParams params);

}  // namespace ash

#endif  // ASH_FAST_INK_VIEW_TREE_HOST_WIDGET_H_
