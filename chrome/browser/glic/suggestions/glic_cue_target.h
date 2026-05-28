// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SUGGESTIONS_GLIC_CUE_TARGET_H_
#define CHROME_BROWSER_GLIC_SUGGESTIONS_GLIC_CUE_TARGET_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/contextual_cueing/cue_target.h"
#include "components/tabs/public/tab_interface.h"

class BrowserWindowInterface;
class OptimizationGuideKeyedService;

namespace glic {

class GlicKeyedService;

class GlicCueTarget : public contextual_cueing::CueTarget {
 public:
  static void Register(BrowserWindowInterface& browser_window_interface);

  explicit GlicCueTarget(
      GlicKeyedService& glic_keyed_service,
      OptimizationGuideKeyedService* optimization_guide_keyed_service,
      BrowserWindowInterface& browser_window_interface);
  ~GlicCueTarget() override;

  // contextual_cueing::CueTarget:
  bool IsEligible() const override;
  void OnClick(contextual_cueing::CueActionData data) override;
  void OnEditPrompt(contextual_cueing::CueActionData data) override;
  ui::ImageModel GetAnchoredMessageIcon() const override;
  ui::ImageModel GetOmniboxChipIcon() const override;
  contextual_cueing::CueActionData CueActionDataFromResponse(
      const optimization_guide::proto::ContextualCue& cue,
      std::vector<tabs::TabHandle> tabs_to_show) const override;
  optimization_guide::proto::ContextualCueingSurface GetSurface()
      const override;

 private:
  void InvokeGlic(contextual_cueing::CueActionData data,
                  bool should_autosubmit);

  tabs::TabHandle GetActiveTabHandle();

  // Unowned and guaranteed to outlive this.
  raw_ref<GlicKeyedService> glic_keyed_service_;
  raw_ptr<OptimizationGuideKeyedService> optimization_guide_keyed_service_;
  raw_ref<BrowserWindowInterface> browser_window_interface_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SUGGESTIONS_GLIC_CUE_TARGET_H_
