// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SKILLS_SKILLS_UPDATE_OBSERVER_H_
#define CHROME_BROWSER_SKILLS_SKILLS_UPDATE_OBSERVER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/skills/proto/skill.pb.h"
#include "content/public/browser/web_contents_observer.h"

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace optimization_guide {
class OptimizationGuideDecider;
enum class OptimizationGuideDecision;
class OptimizationMetadata;
}  // namespace optimization_guide

namespace skills {

class SkillsUpdateObserver : public content::WebContentsObserver {
 public:
  explicit SkillsUpdateObserver(tabs::TabInterface& tab);
  ~SkillsUpdateObserver() override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  void OnTabActivationChanged(bool is_active);

  const skills::proto::SkillsList* contextual_skills() const {
    return contextual_skills_.get();
  }

 private:
  void OnOptimizationGuideDecision(
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);

  void SendContextualSkillsToGlic();

  raw_ref<tabs::TabInterface> tab_;
  raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_decider_;

  bool is_tab_active_ = false;

  std::unique_ptr<skills::proto::SkillsList> contextual_skills_;

  base::WeakPtrFactory<SkillsUpdateObserver> weak_factory_{this};
};

}  // namespace skills

#endif  // CHROME_BROWSER_SKILLS_SKILLS_UPDATE_OBSERVER_H_
