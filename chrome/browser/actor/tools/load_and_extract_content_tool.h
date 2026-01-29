// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_LOAD_AND_EXTRACT_CONTENT_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_LOAD_AND_EXTRACT_CONTENT_TOOL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "components/tabs/public/tab_interface.h"

class GURL;

namespace actor {

// A tool to load a set of URLs in new tabs, extract their content, and then
// close the tabs.
// TODO(b/443954134): Implement properly, only a skeleton for now.
class LoadAndExtractContentTool : public Tool {
 public:
  LoadAndExtractContentTool(TaskId task_id,
                            ToolDelegate& tool_delegate,
                            std::vector<GURL> urls);

  ~LoadAndExtractContentTool() override;

  // actor::Tool:
  void Validate(ToolCallback callback) override;
  void Invoke(ToolCallback callback) override;
  std::string DebugString() const override;
  std::string JournalEvent() const override;
  std::unique_ptr<ObservationDelayController> GetObservationDelayer(
      ObservationDelayController::PageStabilityConfig page_stability_config)
      override;
  tabs::TabHandle GetTargetTab() const override;

 private:
  ToolCallback callback_;

  std::vector<GURL> urls_;

  base::WeakPtrFactory<LoadAndExtractContentTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_LOAD_AND_EXTRACT_CONTENT_TOOL_H_
