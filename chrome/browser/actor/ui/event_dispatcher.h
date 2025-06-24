// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_EVENT_DISPATCHER_H_
#define CHROME_BROWSER_ACTOR_UI_EVENT_DISPATCHER_H_

#include "base/functional/callback.h"
#include "chrome/common/actor.mojom-forward.h"

class Profile;

namespace actor {
class ToolRequest;

class UiEventDispatcher {
 public:
  using UiCompleteCallback = base::OnceCallback<void(mojom::ActionResultPtr)>;

  virtual ~UiEventDispatcher() = default;

  // Should be called before the ToolRequest is actuated.  Callback will be made
  // once the UI has completed its pre-tool.
  virtual void OnPreTool(Profile* profile,
                         const ToolRequest& tool_request,
                         UiCompleteCallback callback) = 0;

  // Should be called after the ToolRequest is actuated.  Callback will be made
  // once the UI has completed its post-tool.
  virtual void OnPostTool(Profile* profile,
                          const ToolRequest& tool_request,
                          UiCompleteCallback callback) = 0;
};

std::unique_ptr<UiEventDispatcher> NewUiEventDispatcher();
}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_UI_EVENT_DISPATCHER_H_
