// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_TOOL_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "chrome/common/actor.mojom-forward.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace actor {

class ObservationDelayController;

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

  // Provides a journal event name.
  virtual std::string JournalEvent() const = 0;

  // Returns an optional delay object that can be used to delay completion of
  // the tool until some external conditions are met. By default, this returns
  // an object that watches for loading navigations and waits until load is
  // completed and a new frame presented.
  virtual std::unique_ptr<ObservationDelayController> GetObservationDelayer(
      content::RenderFrameHost& target_frame) const;

  // Whether or not the tool requires a frame to operate on. Note, this also
  // includes "tab-scoped" tools which are considered to operate on the "main
  // frame" in the tab.
  // TODO(crbug.com/411462297): Temporary until we have a better mechanism for
  // non-frame-scoped tools.
  virtual bool RequiresFrame() const;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_TOOL_H_
