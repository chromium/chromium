// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_LOAD_AND_EXTRACT_CONTENT_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_LOAD_AND_EXTRACT_CONTENT_TOOL_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/common/actor.mojom-forward.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"

class BrowserWindowInterface;
class GURL;

namespace actor {

// A tool to load a set of URLs in new tabs, extract their content, and then
// close the tabs.
// TODO(b/443954134): Implement properly, only a skeleton for now.
class LoadAndExtractContentTool : public Tool {
 public:
  LoadAndExtractContentTool(TaskId task_id,
                            ToolDelegate& tool_delegate,
                            SessionID window_id,
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
  // Should be called once we no longer need to keep the tab open, i.e. it has
  // navigated and we've extracted the content (or encountered an error).
  void OnTabReadyToClose(size_t index, mojom::ActionResultPtr results);

  void OnAllUrlsCompleted(std::vector<mojom::ActionResultPtr> results);

  void OnBrowserDidClose(BrowserWindowInterface* browser);

  ToolCallback invoke_callback_;

  std::vector<GURL> urls_;
  SessionID window_id_;

  std::vector<tabs::TabHandle> tabs_created_;

  std::optional<ObservationDelayController::PageStabilityConfig>
      page_stability_config_;

  // This should be called once for each URL after completion. The result should
  // be ok if all tools succeeded, otherwise it should be the first error
  // encountered.
  base::RepeatingCallback<void(mojom::ActionResultPtr)>
      per_url_completion_callback_;

  base::CallbackListSubscription browser_did_close_subscription_;

  base::WeakPtrFactory<LoadAndExtractContentTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_LOAD_AND_EXTRACT_CONTENT_TOOL_H_
