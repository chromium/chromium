// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_BROWSER_ACTION_UTIL_H_
#define CHROME_BROWSER_ACTOR_BROWSER_ACTION_UTIL_H_

#include <string_view>

namespace content {
class RenderFrameHost;
class RenderWidgetHost;
class WebContents;
}  // namespace content

namespace optimization_guide::proto {
class Action;
class ActionTarget;
class DocumentIdentifier;
}  // namespace optimization_guide::proto

namespace actor {

// Helper to pick out the ActionTarget from the specific type of action in the
// Action proto. Returns nullptr actions which don't contain this
// field (tab-targeting actions).
const optimization_guide::proto::ActionTarget* ExtractTarget(
    const optimization_guide::proto::Action& action);

// Iterates through the frame tree to find the active RenderFrameHost associated
// with a given DocumentIdentifier.
content::RenderFrameHost* GetRenderFrameForDocumentIdentifier(
    content::WebContents& web_contents,
    const std::string_view target_document_token);

// Iterates through the frame tree to find the local root RenderFrameHost
// associated with a given RenderWidgetHost. A local root is a frame that is
// the highest in its frame subtree to be associated with the widget.
content::RenderFrameHost* GetRootFrameForWidget(
    content::WebContents& web_contents,
    content::RenderWidgetHost* rwh);

// Finds local root frame in the given WebContents that's requested by the
// given action. For a tab-targeting action, this returns the current primary
// main frame. For frame-targeting actions, this returns a nullptr if the
// frame is no longer active or has a new document since the action was
// generated. Otherwise it returns the local root RenderFrameHost that contains
// the target node or coordinate.
content::RenderFrameHost* FindTargetLocalRootFrame(
    content::WebContents& web_contents,
    const optimization_guide::proto::Action& action);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_BROWSER_ACTION_UTIL_H_
