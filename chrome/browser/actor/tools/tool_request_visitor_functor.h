// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_TOOL_REQUEST_VISITOR_FUNCTOR_H_
#define CHROME_BROWSER_ACTOR_TOOLS_TOOL_REQUEST_VISITOR_FUNCTOR_H_

#include "chrome/browser/actor/tools/attempt_form_filling_tool_request.h"
#include "chrome/browser/actor/tools/attempt_login_tool_request.h"
#include "chrome/browser/actor/tools/click_tool_request.h"
#include "chrome/browser/actor/tools/drag_and_release_tool_request.h"
#include "chrome/browser/actor/tools/history_tool_request.h"
#include "chrome/browser/actor/tools/media_control_tool_request.h"
#include "chrome/browser/actor/tools/move_mouse_tool_request.h"
#include "chrome/browser/actor/tools/navigate_tool_request.h"
#include "chrome/browser/actor/tools/script_tool_request.h"
#include "chrome/browser/actor/tools/scroll_to_tool_request.h"
#include "chrome/browser/actor/tools/scroll_tool_request.h"
#include "chrome/browser/actor/tools/select_tool_request.h"
#include "chrome/browser/actor/tools/tab_management_tool_request.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/type_tool_request.h"
#include "chrome/browser/actor/tools/wait_tool_request.h"
#include "chrome/browser/actor/tools/window_management_tool_request.h"

namespace actor {

class ToolRequestVisitorFunctor {
 public:
  virtual void Apply(const ActivateTabToolRequest&) = 0;
  virtual void Apply(const ActivateWindowToolRequest&) = 0;
  virtual void Apply(const AttemptLoginToolRequest&) = 0;
  virtual void Apply(const AttemptFormFillingToolRequest&) = 0;
  virtual void Apply(const ClickToolRequest&) = 0;
  virtual void Apply(const CloseTabToolRequest&) = 0;
  virtual void Apply(const CloseWindowToolRequest&) = 0;
  virtual void Apply(const CreateTabToolRequest&) = 0;
  virtual void Apply(const CreateWindowToolRequest&) = 0;
  virtual void Apply(const DragAndReleaseToolRequest&) = 0;
  virtual void Apply(const HistoryToolRequest&) = 0;
  virtual void Apply(const MediaControlToolRequest&) = 0;
  virtual void Apply(const MoveMouseToolRequest&) = 0;
  virtual void Apply(const NavigateToolRequest&) = 0;
  virtual void Apply(const ScriptToolRequest&) = 0;
  virtual void Apply(const ScrollToolRequest&) = 0;
  virtual void Apply(const ScrollToToolRequest&) = 0;
  virtual void Apply(const SelectToolRequest&) = 0;
  virtual void Apply(const TypeToolRequest&) = 0;
  virtual void Apply(const WaitToolRequest&) = 0;
};
}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_TOOL_REQUEST_VISITOR_FUNCTOR_H_
