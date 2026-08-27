// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INDIGO_INDIGO_CUE_TARGET_H_
#define CHROME_BROWSER_INDIGO_INDIGO_CUE_TARGET_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/contextual_cueing/cue_target.h"
#include "components/optimization_guide/proto/features/contextual_cueing.pb.h"
#include "ui/base/models/image_model.h"

namespace content {
class WebContents;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace indigo {

class IndigoService;

class IndigoCueTarget : public contextual_cueing::CueTarget {
 public:
  static void Register(tabs::TabInterface& tab);

  explicit IndigoCueTarget(IndigoService& indigo_service,
                           tabs::TabInterface& tab);
  IndigoCueTarget(const IndigoCueTarget&) = delete;
  IndigoCueTarget& operator=(const IndigoCueTarget&) = delete;
  ~IndigoCueTarget() override;

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

  base::WeakPtr<IndigoCueTarget> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void OnEligibilityChecked(EligibilityCallback callback, bool eligible);
  void GenerateContent(
      base::OnceCallback<void(
          std::optional<optimization_guide::proto::ContextualCue>)> callback);

  const raw_ref<IndigoService> indigo_service_;
  const raw_ref<tabs::TabInterface> tab_;

  base::WeakPtrFactory<IndigoCueTarget> weak_ptr_factory_{this};
};

}  // namespace indigo

#endif  // CHROME_BROWSER_INDIGO_INDIGO_CUE_TARGET_H_
