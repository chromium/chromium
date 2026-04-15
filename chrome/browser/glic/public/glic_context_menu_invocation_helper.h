// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_CONTEXT_MENU_INVOCATION_HELPER_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_CONTEXT_MENU_INVOCATION_HELPER_H_
namespace tabs {
class TabInterface;
}
namespace glic {
// GlicContextMenuInvocationHelper is a utility class to handle Glic-related
// actions triggered from the contextual menu.
class GlicContextMenuInvocationHelper {
 public:
  // Handles the contextual menu click to invoke Glic.
  // |tab| is the tab where the action was triggered.
  static void HandleContextualMenuClick(tabs::TabInterface* tab);
};
}  // namespace glic
#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_CONTEXT_MENU_INVOCATION_HELPER_H_
