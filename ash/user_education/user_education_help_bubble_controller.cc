// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/user_education_help_bubble_controller.h"

#include "ash/user_education/user_education_types.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "components/user_education/common/help_bubble_params.h"
#include "ui/base/interaction/element_identifier.h"

namespace ash {
namespace {

// The singleton instance owned by the `UserEducationController`.
UserEducationHelpBubbleController* g_instance = nullptr;

}  // namespace

UserEducationHelpBubbleController::UserEducationHelpBubbleController() {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

UserEducationHelpBubbleController::~UserEducationHelpBubbleController() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
UserEducationHelpBubbleController* UserEducationHelpBubbleController::Get() {
  return g_instance;
}

bool UserEducationHelpBubbleController::CreateHelpBubble(
    HelpBubbleId help_bubble_id,
    user_education::HelpBubbleParams help_bubble_params,
    ui::ElementIdentifier element_id,
    ui::ElementContext element_context) {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace ash
