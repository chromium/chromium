// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SKILLS_SKILLS_UPDATE_OBSERVER_H_
#define CHROME_BROWSER_SKILLS_SKILLS_UPDATE_OBSERVER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "components/skills/proto/skill.pb.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

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
  DECLARE_USER_DATA(SkillsUpdateObserver);

  explicit SkillsUpdateObserver(tabs::TabInterface& tab);
  ~SkillsUpdateObserver() override;

  // Retrieves a SkillsUpdateObserver* from the given tab, or nullptr if it does
  // not exist.
  static SkillsUpdateObserver* From(tabs::TabInterface* tab);

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  const skills::proto::SkillsList* contextual_skills() const {
    return contextual_skills_.get();
  }

  std::vector<glic::mojom::SkillPtr> GetContextualSkills() const;

 private:
  void OnOptimizationGuideDecision(
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);

  // Finds the correct glic instance (if any) to which contextual skills could
  // be sent and triggers an update if it finds such an instance.
  void MaybeUpdateContextualSkills();

  raw_ref<tabs::TabInterface> tab_;
  raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_decider_;

  std::unique_ptr<skills::proto::SkillsList> contextual_skills_;

  ui::ScopedUnownedUserData<SkillsUpdateObserver> scoped_data_;

  base::WeakPtrFactory<SkillsUpdateObserver> weak_factory_{this};
};

}  // namespace skills

#endif  // CHROME_BROWSER_SKILLS_SKILLS_UPDATE_OBSERVER_H_
