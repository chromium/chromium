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

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/types/strong_alias.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/task_id.h"
#include "components/actor/public/mojom/actor_types.mojom-forward.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace actor {

// A tool to load a set of URLs in new tabs, wait for them to finish navigating
// and be stable, extract their content (i.e. APCs), and then close the tabs.
class LoadAndExtractContentTool : public Tool {
 public:
  LoadAndExtractContentTool(TaskId task_id,
                            ToolDelegate& tool_delegate,
                            SessionID window_id,
                            base::span<const GURL> urls);

  ~LoadAndExtractContentTool() override;

  // actor::Tool:
  void Validate(ToolCallback callback) override;
  void Invoke(ToolCallback callback) override;
  void UpdateTaskAfterInvoke(ActorTask& task,
                             mojom::ActionResultPtr result,
                             ToolCallback callback) const override;
  std::string DebugString() const override;
  std::string JournalEvent() const override;
  std::unique_ptr<ObservationDelayController> GetObservationDelayer(
      ObservationDelayController::PageStabilityConfig page_stability_config)
      override;
  tabs::TabHandle GetTargetTab() const override;

 private:
  struct PerTabState;
  enum class PerTabResultCode;
  class TabObservationDelayer;
  friend class TabObservationDelayer;  // Allow access to PerTabResultCode.

  // Should be called once we no longer need to keep the tab open, i.e. it has
  // navigated and we've extracted the content (or encountered an error).
  void OnTabReadyToClose(size_t index, PerTabResultCode result_code);

  void OnTabObservationDelayComplete(size_t index,
                                     PerTabResultCode result_code);
  void OnGotAIPageContent(
      size_t index,
      optimization_guide::AIPageContentResultOrError result_or_error);
  void OnAllUrlsCompleted();

  optimization_guide::proto::TabObservation::TabObservationResult
  ToTabObservationResult(PerTabResultCode result_code);
  mojom::ActionResultCode ToActionResultCode(
      LoadAndExtractContentTool::PerTabResultCode result_code);

  ToolCallback invoke_callback_;

  SessionID window_id_;

  // Tracks the state of the tab associated with each GURL. The order of the
  // elements in the vector matches the order of the GURLs passed to the
  // constructor.
  std::vector<PerTabState> per_tab_state_;

  std::optional<ObservationDelayController::PageStabilityConfig>
      page_stability_config_;

  // This should be called once for each URL after completion.
  base::RepeatingClosure per_url_completion_closure_;

  base::WeakPtrFactory<LoadAndExtractContentTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_LOAD_AND_EXTRACT_CONTENT_TOOL_H_
