// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_PAGE_TARGET_UTIL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_PAGE_TARGET_UTIL_H_

#include <optional>
#include <string_view>

#include "chrome/browser/actor/shared_types.h"
#include "components/optimization_guide/content/browser/page_content_proto_util.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/tabs/public/tab_interface.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace optimization_guide::proto {
class AnnotatedPageContent;
}  // namespace optimization_guide::proto

namespace actor {

// Returns the `RenderFrameHost` for a `PageTarget`.
content::RenderFrameHost* FindTargetLocalRootFrame(tabs::TabHandle tab_handle,
                                                   PageTarget target);

// Return `TargetNodeInfo` from hit test against the last observed APC. Returns
// std::nullopt if Target does not hit any node.
std::optional<optimization_guide::TargetNodeInfo>
FindLastObservedNodeForActionTargetId(
    const optimization_guide::proto::AnnotatedPageContent* apc,
    const DomNode& target);

std::optional<optimization_guide::TargetNodeInfo>
FindLastObservedNodeForActionTargetPoint(
    const optimization_guide::proto::AnnotatedPageContent* apc,
    const gfx::Point& target_pixels);

std::optional<optimization_guide::TargetNodeInfo>
FindLastObservedNodeForActionTarget(
    const optimization_guide::proto::AnnotatedPageContent* apc,
    const PageTarget& target);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_PAGE_TARGET_UTIL_H_
