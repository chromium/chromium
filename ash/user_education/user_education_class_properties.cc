// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/user_education_class_properties.h"

#include "ui/gfx/geometry/insets.h"

DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(ASH_EXPORT, ash::HelpBubbleContext)

namespace ash {

DEFINE_UI_CLASS_PROPERTY_KEY(HelpBubbleContext,
                             kHelpBubbleContextKey,
                             HelpBubbleContext::kDefault)

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::Insets, kPingInsetsKey, nullptr)

}  // namespace ash
