// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SELECTION_SUGGESTION_SERVICE_H_
#define CHROME_BROWSER_SELECTION_SUGGESTION_SERVICE_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/selection/suggestion.h"
#include "chrome/browser/selection/suggestion_tool.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace content {
class WebContents;
}

namespace optimization_guide {
class ModelQualityLogEntry;
struct OptimizationGuideModelExecutionResult;
class RemoteModelExecutor;
namespace proto {
class SmartSelectionSuggestionsRequest;
}  // namespace proto
}  // namespace optimization_guide

namespace tabs {
class TabInterface;
}

namespace selection {

// TabFeature associated service for requesting zero-state or context-aware
// selection suggestions from registered feature tools.
class SuggestionService {
 public:
  DECLARE_USER_DATA(SuggestionService);

  SuggestionService(
      tabs::TabInterface* tab,
      optimization_guide::RemoteModelExecutor* remote_model_executor);
  virtual ~SuggestionService();

  SuggestionService(const SuggestionService&) = delete;
  SuggestionService& operator=(const SuggestionService&) = delete;

  static SuggestionService* From(tabs::TabInterface* tab);
  static SuggestionService* FromTabWebContents(
      content::WebContents* tab_web_contents);

  // Registers `tool`. There must not be a tool with the same `ToolId` already
  // registered.
  void RegisterTool(SuggestionTool* tool);
  void UnregisterTool(SuggestionTool* tool);

  // Updates the screen content (screenshot and APC) for the active tab.
  void UpdateScreenContent(
      const SkBitmap& screenshot,
      const optimization_guide::proto::AnnotatedPageContent& apc);

  // Requests zero-state or context-aware suggestions from registered tools
  // for the active selection context in the tab's WebContents.
  virtual void RequestSuggestions(const AreaOfInterest& processed_area,
                                  SuggestionsCallback callback);

 private:
  struct ActiveRequest;

  // Requests suggestions for `active_request` from MES.
  void RequestServerSuggestions(scoped_refptr<ActiveRequest> active_request);

  // Attaches `png_bytes` (if any) to `request` and sends it to MES.
  void SendServerSuggestionsRequest(
      scoped_refptr<ActiveRequest> active_request,
      optimization_guide::proto::SmartSelectionSuggestionsRequest request,
      std::optional<std::vector<uint8_t>> png_bytes);

  // Processes the server response by calling tools to convert the response into
  // `Suggestion`s and calling the initial `callback` with them.
  void OnServerSuggestions(
      scoped_refptr<ActiveRequest> active_request,
      optimization_guide::OptimizationGuideModelExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);

  void OnToolSuggestions(
      scoped_refptr<ActiveRequest> active_request,
      bool& tool_completed,
      std::vector<std::unique_ptr<Suggestion>> suggestions,
      bool complete);

  const raw_ref<tabs::TabInterface> tab_;
  const raw_ptr<optimization_guide::RemoteModelExecutor> remote_model_executor_;
  base::flat_map<SuggestionTool::ToolId, raw_ptr<SuggestionTool>> tools_;

  ui::ScopedUnownedUserData<SuggestionService> scoped_unowned_user_data_;

  base::WeakPtrFactory<SuggestionService> weak_factory_{this};
};

}  // namespace selection

#endif  // CHROME_BROWSER_SELECTION_SUGGESTION_SERVICE_H_

