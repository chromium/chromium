// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ACTIONS_CHROME_ACTION_PROPERTIES_H_
#define CHROME_BROWSER_UI_ACTIONS_CHROME_ACTION_PROPERTIES_H_

#include "ui/base/class_property.h"
#include "ui/base/window_open_disposition.h"

class GURL;

namespace url {
class Origin;
}

namespace chrome {

enum class ActionInvocationSource {
  kUnknown = 0,
  kKeyboardShortcut,
  kViewButton,
  kContextMenu,
  kOmniboxChip,
};

// The disposition (e.g., current tab, new tab, new window) to use.
extern const ui::ClassProperty<WindowOpenDisposition>* const kDispositionKey;

// The invocation source (e.g., keyboard shortcut, view button).
extern const ui::ClassProperty<ActionInvocationSource>* const
    kActionInvocationSourceKey;

// The target URL of a link.
extern const ui::ClassProperty<const GURL*>* const kLinkUrlKey;

// The URL of the frame containing the context menu.
extern const ui::ClassProperty<const GURL*>* const kFrameUrlKey;
// The origin of the frame containing the context menu.
extern const ui::ClassProperty<const url::Origin*>* const kFrameOriginKey;
// The referrer policy of the frame containing the context menu.
extern const ui::ClassProperty<int>* const kReferrerPolicyKey;

}  // namespace chrome

DECLARE_UI_CLASS_PROPERTY_TYPE(WindowOpenDisposition)
DECLARE_UI_CLASS_PROPERTY_TYPE(const GURL*)
DECLARE_UI_CLASS_PROPERTY_TYPE(const url::Origin*)
DECLARE_UI_CLASS_PROPERTY_TYPE(chrome::ActionInvocationSource)

#endif  // CHROME_BROWSER_UI_ACTIONS_CHROME_ACTION_PROPERTIES_H_
