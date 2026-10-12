// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_UTIL_H_
#define CHROME_BROWSER_ACTOR_ACTOR_UTIL_H_

#include <optional>
#include <string_view>
#include <vector>

#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "content/public/browser/render_frame_host.h"

namespace content {
class WebContents;
}

namespace glic {
class GlicInstance;
}

namespace actor {

class ActorTask;

// Returns the GlicInstance associated with `task`, or nullptr if `task` is not
// associated with a Glic instance.
glic::GlicInstance* GetGlicInstanceForTask(const ActorTask& task);

// Returns whether `task` is associated with a Glic instance.
bool HasGlicInstanceForTask(const ActorTask& task);

// Returns whether the given WebContents is associated with an actor task.
bool HaveActiveTaskForContents(content::WebContents* source_contents);

// Returns true if the given WebContents is associated with an actor task and
// the task is running in the background.
// This is based not just on whether the tab for the task is active, but also on
// the Glic instance that could be seen by the user. If the Glic instance
// associated with the task is showing, then the task is not considered to be in
// the background.
bool IsRunningBackgroundActorTask(content::WebContents& source_contents);

// Returns true if the given RenderFrameHost belongs to a tab being controlled
// by an actor task and if the task's current state disallows opening new web
// contents.
// TODO(b/420669167): Remove this.
bool HasActorTaskPreventingNewWebContents(content::RenderFrameHost* rfh);

// Returns an encoded screenshot, which is a copy of `screenshot_data` with
// bounding boxes drawn around iframes, as specified in `screenshot_info`.
// Returns std::nullopt if `screenshot_data` cannot be decoded, or if the
// resulting image cannot be encoded.
std::optional<std::vector<uint8_t>> GetScreenshotWithIframeBoundingBoxes(
    const std::vector<uint8_t>& screenshot_data,
    std::string_view mime_type,
    const optimization_guide::proto::ScreenshotInfo& screenshot_info);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_UTIL_H_
