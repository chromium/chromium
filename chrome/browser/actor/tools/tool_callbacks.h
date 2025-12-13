// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_TOOL_CALLBACKS_H_
#define CHROME_BROWSER_ACTOR_TOOLS_TOOL_CALLBACKS_H_

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "chrome/common/actor.mojom-forward.h"

namespace actor {
using ToolCallback = base::OnceCallback<void(mojom::ActionResultPtr)>;

// Helper to post a callback on the current sequence with the given response.
void PostResponseTask(ToolCallback task,
                      mojom::ActionResultPtr result,
                      base::TimeDelta delay = base::Seconds(0));

// Helper to post a "finished" callback to notify the caller of completion.
void PostFinishedTask(base::OnceClosure task,
                      base::TimeDelta delay = base::Seconds(0));

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_TOOL_CALLBACKS_H_
