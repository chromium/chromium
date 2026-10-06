// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_CUE_TARGET_H_
#define CHROME_BROWSER_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_CUE_TARGET_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/contextual_cueing/cue_target.h"
#include "components/page_content_annotations/core/page_content_annotations_common.h"
#include "url/gurl.h"

class OptimizationGuideKeyedService;

namespace contextual_tasks {
class DesktopQueryContextualizerDelegate;
class QueryContextualizer;
}  // namespace contextual_tasks

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace contextual_search {

class ContextualSearchSessionHandle;

class ContextualSearchCueTarget : public contextual_cueing::CueTarget {
 public:
  static void Register(tabs::TabInterface& tab);

  ContextualSearchCueTarget(
      OptimizationGuideKeyedService* optimization_guide_keyed_service,
      tabs::TabInterface& tab);
  ~ContextualSearchCueTarget() override;

  // contextual_cueing::CueTarget:
  contextual_cueing::CueTargetType GetType() const override;
  bool RequiresModelExecution() const override;
  bool IsEligible() const override;
  void CheckEligibility(base::WeakPtr<content::WebContents> web_contents,
                        contextual_cueing::CueIntrusiveness intrusiveness,
                        EligibilityCallback callback) override;
  bool IsPageEligible(
      const page_content_annotations::PageContentAnnotationsResult& result,
      content::WebContents* active_web_contents) const override;
  void OnAnchoredMessageClicked(contextual_cueing::CueActionData data) override;
  bool SupportsEditPrompt() const override;
  void OnEditPrompt(contextual_cueing::CueActionData data) override;
  ui::ImageModel GetAnchoredMessageIcon() const override;
  ui::ImageModel GetOmniboxChipIcon() const override;
  contextual_cueing::CueActionData CueActionDataFromResponse(
      const optimization_guide::proto::ContextualCue& cue,
      std::vector<tabs::TabHandle> tabs_to_show) const override;
  optimization_guide::proto::ContextualCueingSurface GetSurface()
      const override;

  base::WeakPtr<ContextualSearchCueTarget> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  ContextualSearchSessionHandle* GetSessionHandle();
  void OnContextualizationComplete(
      std::string query,
      base::WeakPtr<ContextualSearchSessionHandle> session_handle);
  void OpenContextualTasksSidePanel(GURL url);

  raw_ptr<OptimizationGuideKeyedService> optimization_guide_keyed_service_;
  raw_ref<tabs::TabInterface> tab_;

  std::unique_ptr<ContextualSearchSessionHandle> session_handle_;
  std::unique_ptr<contextual_tasks::DesktopQueryContextualizerDelegate>
      query_contextualizer_delegate_;
  std::unique_ptr<contextual_tasks::QueryContextualizer> query_contextualizer_;

  base::WeakPtrFactory<ContextualSearchCueTarget> weak_ptr_factory_{this};
};

}  // namespace contextual_search

#endif  // CHROME_BROWSER_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_CUE_TARGET_H_
