// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/hud_display/graph_page_view_base.h"

namespace ash {
namespace hud_display {

BEGIN_METADATA(GraphPageViewBase, View)
END_METADATA

GraphPageViewBase::GraphPageViewBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
}

GraphPageViewBase::~GraphPageViewBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
}

}  // namespace hud_display
}  // namespace ash
