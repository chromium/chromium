// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_GET_POINTER_TOOL_H_
#define CHROME_BROWSER_TTC_CORE_GET_POINTER_TOOL_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ttc/core/pointer_context_capturer.h"
#include "chrome/browser/ttc/core/pointer_tracker.h"
#include "components/ttc/app/public/tool_types.h"

namespace content {
class Page;
}  // namespace content

namespace ttc {

class SessionControllerImpl;

// The get_pointer tool of a session: tells the model what the user pointed at
// during their turn, with an image of the page around it and a description of
// the element there. See BuildGetPointerResponse() for the answer.
//
// TODO(crbug.com/571268337): Make this an actor tool once actor tools can
// return data and an image to the session; today they return a result code
// and a message. The same holds for get_screenshot.
class GetPointerTool {
 public:
  // The tool's feature must be enabled. `session_controller` must outlive this
  // object, and track the pointer (see
  // SessionControllerImpl::pointer_tracker()) by the time the tool is run.
  explicit GetPointerTool(SessionControllerImpl& session_controller);
  GetPointerTool(const GetPointerTool&) = delete;
  GetPointerTool& operator=(const GetPointerTool&) = delete;
  ~GetPointerTool();

  // What the model is told about the tool.
  static ToolDefinition GetDefinition();

  // Answers a call of the tool through `callback`. Captures the page the
  // session is focused on if the user pointed at something and TTC may read
  // the page.
  void Run(ToolResponseCallback callback);

 private:
  void OnUrlChecked(base::WeakPtr<content::Page> page,
                    std::vector<PointerTracker::Location> locations,
                    ToolResponseCallback callback,
                    bool is_url_supported);
  void OnCaptured(base::WeakPtr<content::Page> page,
                  std::vector<PointerTracker::Location> locations,
                  ToolResponseCallback callback,
                  PointerContextCapturer::Result context);
  void OnResponseBuilt(base::WeakPtr<content::Page> page,
                       ToolResponseCallback callback,
                       ToolResponse response);

  const raw_ref<SessionControllerImpl> session_controller_;

  // Set while the page is being captured.
  std::unique_ptr<PointerContextCapturer> capturer_;

  base::WeakPtrFactory<GetPointerTool> weak_factory_{this};
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_GET_POINTER_TOOL_H_
