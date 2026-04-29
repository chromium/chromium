// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_UTIL_H_
#define CHROME_BROWSER_ACTOR_ACTOR_UTIL_H_

#include "content/public/browser/render_frame_host.h"

namespace content {
class WebContents;
}

namespace actor {

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

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_UTIL_H_
