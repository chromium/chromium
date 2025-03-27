// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_TOOL_CALLBACKS_H_
#define CHROME_BROWSER_ACTOR_TOOLS_TOOL_CALLBACKS_H_

#include "base/functional/callback_forward.h"

namespace actor {

// Helper to post a callback on the current sequence with the given response.
// TODO(crbug.com/389739308): This will have to be split up or templated when
// responses become typed rather than the placeholder bool.
void PostResponseTask(base::OnceCallback<void(bool)> task, bool response);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_TOOL_CALLBACKS_H_
