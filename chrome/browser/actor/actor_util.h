// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_UTIL_H_
#define CHROME_BROWSER_ACTOR_ACTOR_UTIL_H_

namespace content {
class WebContents;
}

namespace actor {

// Whether actor safety checks are disabled.
bool IsActorSafetyCheckDisabled();

// Whether actor navigation gating is enabled.
bool IsNavigationGatingEnabled();

// Returns whether the given WebContents is associated with an actor task.
bool HaveActiveTaskForContents(content::WebContents* source_contents);

// Returns true if the given WebContents is associated with an actor task and
// the task is running in the background.
bool IsRunningBackgroundActorTask(content::WebContents& source_contents);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_UTIL_H_
