// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_USER_EDUCATION_CLASS_PROPERTIES_H_
#define ASH_USER_EDUCATION_USER_EDUCATION_CLASS_PROPERTIES_H_

#include "ash/ash_export.h"
#include "ui/base/class_property.h"

namespace ash {

// TODO(http://b/277994050): Remove after Lacros launch.
// A property which can be set on a tracked element to indicate its context to
// help bubble factories. Help bubble factories may use context to determine
// whether to create a bubble. Currently used to allow Ash-specific help bubbles
// to take precedence over standard Views-specific help bubbles in System UI.
// NOTE: Set `kHelpBubbleContextKey` before `views::kElementIdentifierKey` in
// case registration causes a help bubble to be created synchronously.
enum class HelpBubbleContext { kDefault, kAsh };
ASH_EXPORT extern const ui::ClassProperty<HelpBubbleContext>* const
    kHelpBubbleContextKey;

}  // namespace ash

DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(ASH_EXPORT, ash::HelpBubbleContext)

#endif  // ASH_USER_EDUCATION_USER_EDUCATION_CLASS_PROPERTIES_H_
