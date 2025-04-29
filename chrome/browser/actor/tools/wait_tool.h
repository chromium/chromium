// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_WAIT_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_WAIT_TOOL_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/tools/tool.h"

namespace actor {

// Waits for a page to settle before continuing with other tools.
class WaitTool : public Tool {
 public:
  WaitTool();
  ~WaitTool() override;

  // actor::Tool
  void Validate(ValidateCallback callback) override;
  void Invoke(InvokeCallback callback) override;
  std::string DebugString() const override;
  bool ShouldAddCompletionDelay() const override;

  static void SetNoDelayForTesting();

 private:
  void OnDelayFinished(InvokeCallback callback);

  static bool no_delay_for_testing_;

  base::WeakPtrFactory<WaitTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_WAIT_TOOL_H_
