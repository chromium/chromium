// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/skills/skills_update_observer.h"

#include "chrome/browser/glic/host/glic_skills_manager.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "components/optimization_guide/core/hints/optimization_guide_decider.h"
#include "components/optimization_guide/core/hints/optimization_guide_decision.h"
#include "components/optimization_guide/core/hints/optimization_metadata.h"
#include "components/skills/features.h"
#include "components/skills/proto/skill.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace {
std::vector<glic::mojom::SkillPtr> ConvertSkillsListToSkills(
    const skills::proto::SkillsList* skills_list) {
  std::vector<glic::mojom::SkillPtr> skills;
  if (!skills_list) {
    return skills;
  }
  for (const skills::proto::Skill& skill_proto : skills_list->skills()) {
    glic::mojom::SkillPreviewPtr skill_preview =
        glic::mojom::SkillPreview::New();
    skill_preview->id = skill_proto.id();
    skill_preview->name = skill_proto.name();
    skill_preview->icon = skill_proto.icon();
    skill_preview->source = glic::mojom::SkillSource::kFirstParty;

    glic::mojom::SkillPtr skill = glic::mojom::Skill::New();
    skill->preview = std::move(skill_preview);
    skill->prompt = skill_proto.prompt();
    skills.push_back(std::move(skill));
  }
  return skills;
}
}  // namespace

namespace skills {

DEFINE_USER_DATA(SkillsUpdateObserver);

SkillsUpdateObserver::SkillsUpdateObserver(tabs::TabInterface& tab)
    : content::WebContentsObserver(tab.GetContents()),
      tab_(tab),
      optimization_guide_decider_(
          OptimizationGuideKeyedServiceFactory::GetForProfile(
              Profile::FromBrowserContext(
                  tab.GetContents()->GetBrowserContext()))),
      scoped_data_(tab.GetUnownedUserDataHost(), *this) {}

SkillsUpdateObserver::~SkillsUpdateObserver() = default;

// static
SkillsUpdateObserver* SkillsUpdateObserver::From(tabs::TabInterface* tab) {
  if (!tab) {
    return nullptr;
  }
  return Get(tab->GetUnownedUserDataHost());
}

void SkillsUpdateObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!base::FeatureList::IsEnabled(features::kSkillsEnabled)) {
    return;
  }

  if (!optimization_guide_decider_) {
    return;
  }

  if (!navigation_handle->HasCommitted() ||
      !navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  optimization_guide_decider_->CanApplyOptimization(
      navigation_handle->GetURL(),
      optimization_guide::proto::OptimizationType::SKILLS,
      base::BindOnce(&SkillsUpdateObserver::OnOptimizationGuideDecision,
                     weak_factory_.GetWeakPtr()));
}

void SkillsUpdateObserver::OnOptimizationGuideDecision(
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  contextual_skills_.reset();
  if (decision != optimization_guide::OptimizationGuideDecision::kTrue) {
    return;
  }
  std::optional<skills::proto::SkillsList> skills_list =
      metadata.ParsedMetadata<skills::proto::SkillsList>();
  if (!skills_list.has_value()) {
    return;
  }
  contextual_skills_ = std::make_unique<skills::proto::SkillsList>(
      std::move(skills_list.value()));
  MaybeUpdateContextualSkills();
}

void SkillsUpdateObserver::MaybeUpdateContextualSkills() {
  glic::GlicKeyedService* glic_keyed_service = glic::GlicKeyedService::Get(
      Profile::FromBrowserContext(tab_->GetContents()->GetBrowserContext()));
  if (!glic_keyed_service) {
    return;
  }
  if (glic::GlicInstance* instance =
          glic_keyed_service->GetInstanceForTab(&(*tab_))) {
    instance->host().skills_manager().UpdateSkillPreviews(&(*tab_));
  }
}

std::vector<glic::mojom::SkillPtr>
SkillsUpdateObserver::GetContextualSkills() const {
  return ConvertSkillsListToSkills(contextual_skills_.get());
}

}  // namespace skills
