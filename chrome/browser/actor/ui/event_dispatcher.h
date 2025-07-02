// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_EVENT_DISPATCHER_H_
#define CHROME_BROWSER_ACTOR_UI_EVENT_DISPATCHER_H_

#include "base/functional/callback.h"
#include "chrome/browser/actor/task_id.h"
#include "chrome/common/actor.mojom-forward.h"
#include "components/tabs/public/tab_interface.h"

class Profile;

namespace actor {
class ToolRequest;
namespace ui {

// This object is not thread safe; it expects to be called from a single thread.
class UiEventDispatcher {
 public:
  struct FirstActInfo {
    TaskId task_id;
    std::optional<tabs::TabInterface::Handle> tab_handle;
  };

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

  // Should be called before the first ToolRequest is processed.  Callback will
  // be made once the UI has initialized.
  virtual void OnPreFirstAct(Profile* profile,
                             const FirstActInfo& first_act_info,
                             UiCompleteCallback callback) = 0;
};

std::unique_ptr<UiEventDispatcher> NewUiEventDispatcher();
}  // namespace ui
}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_UI_EVENT_DISPATCHER_H_
