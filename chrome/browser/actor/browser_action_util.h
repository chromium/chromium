// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_BROWSER_ACTION_UTIL_H_
#define CHROME_BROWSER_ACTOR_BROWSER_ACTION_UTIL_H_

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace optimization_guide::proto {
class ActionInformation;
class ActionTarget;
}  // namespace optimization_guide::proto

namespace actor {

// Helper to pick out the ActionTarget from the specific type of action in the
// ActionInformation proto. Returns nullptr actions which don't contain this
// field (tab-targeting actions).
const optimization_guide::proto::ActionTarget* ExtractTarget(
    const optimization_guide::proto::ActionInformation& action_information);

// Finds the specific frame in the given WebContents that's requested by the
// given action. For a tab-targeting action, this returns the current primary
// main frame. For frame-targeting actions, this returns null if the frame is no
// longer active or has a new document since the action was generated.
content::RenderFrameHost* FindTargetFrame(
    content::WebContents& web_contents,
    const optimization_guide::proto::ActionInformation& action_information);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_BROWSER_ACTION_UTIL_H_
