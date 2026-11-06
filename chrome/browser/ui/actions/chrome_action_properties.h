// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ACTIONS_CHROME_ACTION_PROPERTIES_H_
#define CHROME_BROWSER_UI_ACTIONS_CHROME_ACTION_PROPERTIES_H_

#include <string>

#include "ui/base/class_properties.h"
#include "ui/base/class_property.h"
#include "ui/base/mojom/window_open_disposition.mojom-forward.h"

namespace chrome {

enum class ActionInvocationSource {
  kUnknown = 0,
  kKeyboardShortcut,
  kViewButton,
  kContextMenu,
  kOmniboxChip,
  kAppMenu,
};

// The disposition (e.g., current tab, new tab, new window) to use.
extern const ui::ClassProperty<ui::mojom::WindowOpenDisposition>* const
    kDispositionKey;

// The invocation source (e.g., keyboard shortcut, view button).
extern const ui::ClassProperty<ActionInvocationSource>* const
    kActionInvocationSourceKey;

// Delta (in dp) applied to the default icon size when rendering an ActionItem's
// icon in pinned toolbar buttons and the Customize Chrome toolbar list. Must be
// non-positive: the Customize Chrome toolbar list scales every icon to fit the
// same box, so an icon can only be rendered smaller than its siblings, never
// larger.
extern const ui::ClassProperty<int>* const kActionItemIconSizeDeltaKey;

// Secondary text that surfaces may show under the action's title, kept current
// by whoever owns the underlying state (e.g. "Zoom - 110%" on the zoom in/out
// actions, updated on every zoom change). Unset when there is nothing to show.
extern const ui::ClassProperty<std::u16string*>* const kActionSecondaryTextKey;

}  // namespace chrome

DECLARE_UI_CLASS_PROPERTY_TYPE(ui::mojom::WindowOpenDisposition)
DECLARE_UI_CLASS_PROPERTY_TYPE(chrome::ActionInvocationSource)

#endif  // CHROME_BROWSER_UI_ACTIONS_CHROME_ACTION_PROPERTIES_H_
