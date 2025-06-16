// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_TAB_MANAGEMENT_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_TAB_MANAGEMENT_TOOL_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/tools/tool.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"

namespace actor {

// A tool to manage the tabs in a browser window, e.g. create, close,
// activate, etc.
// TODO(crbug.com/411462297): Implement actions other than create.
class TabManagementTool : public Tool {
 public:
  TabManagementTool(int32_t window_id,
                    const optimization_guide::proto::CreateTabAction& action);
  ~TabManagementTool() override;

  // actor::Tool:
  void Validate(ValidateCallback callback) override;
  void Invoke(InvokeCallback callback) override;
  std::string DebugString() const override;
  std::string JournalEvent() const override;
  bool RequiresFrame() const override;

 private:
  int32_t window_id_;
  const optimization_guide::proto::CreateTabAction action_;

  base::WeakPtrFactory<TabManagementTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_TAB_MANAGEMENT_TOOL_H_
