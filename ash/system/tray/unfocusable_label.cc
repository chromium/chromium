// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/unfocusable_label.h"

#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

UnfocusableLabel::UnfocusableLabel() {
  SetFocusBehavior(View::FocusBehavior::NEVER);
}

UnfocusableLabel::~UnfocusableLabel() = default;

BEGIN_METADATA(UnfocusableLabel)
END_METADATA

}  // namespace ash
