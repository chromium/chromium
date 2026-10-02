// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_CONTEXT_MENU_INVOCATION_HELPER_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_CONTEXT_MENU_INVOCATION_HELPER_H_

#include <string>

#include "content/public/browser/global_routing_id.h"

namespace content {
class RenderFrameHost;
struct ContextMenuParams;
}  // namespace content
namespace tabs {
class TabInterface;
}
namespace glic {

inline constexpr char kMimeTypeGlicSelection[] = "application/x-glic-selection";

// GlicContextMenuInvocationHelper is a utility class to handle Glic-related
// actions triggered from the contextual menu.
class GlicContextMenuInvocationHelper {
 public:
  // Handles the contextual menu click to invoke Glic.
  // |tab| is the tab where the action was triggered.
  static void HandleContextualMenuClick(
      tabs::TabInterface* tab,
      const std::u16string& selection_text = std::u16string(),
      content::GlobalRenderFrameHostId rfh_id =
          content::GlobalRenderFrameHostId());

  // Handles the context menu click to get a smart suggestion.
  // Pre-selects a region around the content described by `params` when
  // possible.
  static void HandleSmartSuggestionClick(
      content::RenderFrameHost& frame,
      const content::ContextMenuParams& params);
};
}  // namespace glic
#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_CONTEXT_MENU_INVOCATION_HELPER_H_
