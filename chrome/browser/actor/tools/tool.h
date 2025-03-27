// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_TOOL_H_

#include "base/functional/callback_forward.h"

namespace actor {

// Interface all actor tools implement. A tool is held by the ToolController and
// validated and invoked from there. The controller makes no guarantees about
// when the tool will be destroyed.
class Tool {
 public:
  using ValidateCallback = base::OnceCallback<void(bool)>;
  using InvokeCallback = base::OnceCallback<void(bool)>;
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
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_TOOL_H_
