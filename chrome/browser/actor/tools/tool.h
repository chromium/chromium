// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_TOOL_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "chrome/common/actor.mojom-forward.h"

namespace actor {

// Interface all actor tools implement. A tool is held by the ToolController and
// validated and invoked from there. The controller makes no guarantees about
// when the tool will be destroyed.
class Tool {
 public:
  // NOTE: Let's rename this to `ToolCallback`, move to a shared header, and
  // eliminate the other redundant definitions.
  using ValidateCallback = base::OnceCallback<void(mojom::ActionResultPtr)>;
  using InvokeCallback = base::OnceCallback<void(mojom::ActionResultPtr)>;
  Tool() = default;
  virtual ~Tool() = default;

  // Perform any browser-side validation on the tool. The given callback must be
  // invoked by the tool when validation is completed. If the result given to
  // the callback indicates success, the framework will call Invoke. Otherwise,
  // the tool will be destroyed.
  virtual void Validate(ValidateCallback callback) = 0;

  // Perform the action of the tool. The given callback must be invoked when the
  // tool has finished its actions.
  virtual void Invoke(InvokeCallback callback) = 0;

  // Provides a human readable description of the tool useful for log and
  // debugging purposes.
  virtual std::string DebugString() const = 0;

  // Returns true if the completion of this tool should be artificially delayed
  // to allow async work triggered by the tool to finish.
  virtual bool ShouldAddCompletionDelay() const;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_TOOL_H_
