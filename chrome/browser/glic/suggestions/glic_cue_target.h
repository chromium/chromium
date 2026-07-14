// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SUGGESTIONS_GLIC_CUE_TARGET_H_
#define CHROME_BROWSER_GLIC_SUGGESTIONS_GLIC_CUE_TARGET_H_

#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/contextual_cueing/cue_target.h"
#include "components/page_content_annotations/core/page_content_annotations_common.h"

class OptimizationGuideKeyedService;

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace glic {

class GlicKeyedService;

class GlicCueTarget : public contextual_cueing::CueTarget {
 public:
  static void Register(tabs::TabInterface& tab);

  explicit GlicCueTarget(
      GlicKeyedService& glic_keyed_service,
      OptimizationGuideKeyedService* optimization_guide_keyed_service,
      tabs::TabInterface& tab);
  ~GlicCueTarget() override;

  // contextual_cueing::CueTarget:
  contextual_cueing::CueTargetType GetType() const override;
  bool IsEligible() const override;
  void CheckEligibility(base::WeakPtr<content::WebContents> web_contents,
                        contextual_cueing::CueIntrusiveness intrusiveness,
                        EligibilityCallback callback) override;
  bool IsPageEligible(
      const page_content_annotations::PageContentAnnotationsResult& result,
      content::WebContents* active_web_contents) const override;
  void OnClick(contextual_cueing::CueActionData data) override;
  void OnEditPrompt(contextual_cueing::CueActionData data) override;
  ui::ImageModel GetAnchoredMessageIcon() const override;
  ui::ImageModel GetOmniboxChipIcon() const override;
  contextual_cueing::CueActionData CueActionDataFromResponse(
      const optimization_guide::proto::ContextualCue& cue,
      std::vector<tabs::TabHandle> tabs_to_show) const override;
  optimization_guide::proto::ContextualCueingSurface GetSurface()
      const override;

  base::WeakPtr<GlicCueTarget> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  friend class GlicCueTargetAsyncTest;

  void InvokeGlic(contextual_cueing::CueActionData data,
                  bool should_autosubmit);

  // Unowned and guaranteed to outlive this.
  raw_ref<GlicKeyedService> glic_keyed_service_;
  raw_ptr<OptimizationGuideKeyedService> optimization_guide_keyed_service_;
  raw_ref<tabs::TabInterface> tab_;

  base::WeakPtrFactory<GlicCueTarget> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SUGGESTIONS_GLIC_CUE_TARGET_H_
