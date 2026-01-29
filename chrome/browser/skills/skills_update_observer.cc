// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/skills/skills_update_observer.h"

#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/core/hints/optimization_guide_decider.h"
#include "components/optimization_guide/core/hints/optimization_guide_decision.h"
#include "components/optimization_guide/core/hints/optimization_metadata.h"
#include "components/skills/features.h"
#include "components/skills/proto/skill.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace {
std::vector<glic::mojom::SkillPreviewPtr> ConvertSkillsListToSkillPreviews(
    const skills::proto::SkillsList* skills_list) {
  std::vector<glic::mojom::SkillPreviewPtr> skill_previews;
  if (!skills_list) {
    return skill_previews;
  }
  for (const skills::proto::Skill& skill : skills_list->skills()) {
    glic::mojom::SkillPreviewPtr skill_preview =
        glic::mojom::SkillPreview::New();
    skill_preview->id = skill.id();
    skill_preview->name = skill.name();
    skill_preview->icon = skill.icon();
    skill_preview->source = glic::mojom::SkillSource::kFirstParty;
    skill_previews.push_back(std::move(skill_preview));
  }
  return skill_previews;
}
}  // namespace

namespace skills {

SkillsUpdateObserver::SkillsUpdateObserver(tabs::TabInterface& tab)
    : content::WebContentsObserver(tab.GetContents()),
      tab_(tab),
      optimization_guide_decider_(
          OptimizationGuideKeyedServiceFactory::GetForProfile(
              Profile::FromBrowserContext(
                  tab.GetContents()->GetBrowserContext()))) {}

SkillsUpdateObserver::~SkillsUpdateObserver() = default;

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

void SkillsUpdateObserver::OnTabActivationChanged(bool is_active) {
  is_tab_active_ = is_active;
  SendContextualSkillsToGlic();
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
}

void SkillsUpdateObserver::SendContextualSkillsToGlic() {
  if (!is_tab_active_) {
    return;
  }
  glic::GlicKeyedService* glic_keyed_service = glic::GlicKeyedService::Get(
      Profile::FromBrowserContext(tab_->GetContents()->GetBrowserContext()));
  if (!glic_keyed_service) {
    return;
  }
  glic::GlicInstance* instance =
      glic_keyed_service->GetInstanceForTab(&(*tab_));
  if (!instance || !instance->host().IsReady()) {
    return;
  }
  std::vector<glic::mojom::SkillPreviewPtr> skill_previews =
      ConvertSkillsListToSkillPreviews(contextual_skills_.get());
  // TODO(b:479620618): plumb to glic, when possible.
}

}  // namespace skills
