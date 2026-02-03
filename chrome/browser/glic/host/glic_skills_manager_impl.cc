// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_skills_manager_impl.h"

#include "base/functional/bind.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/skills/skills_update_observer.h"
#include "components/tabs/public/tab_interface.h"

namespace glic {

GlicSkillsManagerImpl::GlicSkillsManagerImpl(Host* host) : host_(*host) {
  focused_tab_changed_subscription_ =
      host->sharing_manager().AddFocusedTabChangedCallback(
          base::BindRepeating(&GlicSkillsManagerImpl::OnFocusedTabChanged,
                              weak_ptr_factory_.GetWeakPtr()));
  host_observation_.Observe(host);
}

GlicSkillsManagerImpl::~GlicSkillsManagerImpl() = default;

void GlicSkillsManagerImpl::UpdateSkillPreviews(
    std::optional<tabs::TabInterface*> updated_tab) {
  if (!host_->IsReady()) {
    return;
  }
  auto* focused_tab = host_->sharing_manager().GetFocusedTabData().focus();
  if (!focused_tab) {
    host_->NotifyContextualSkillsChanged({});
    return;
  }
  if (updated_tab && focused_tab != *updated_tab) {
    // The update does not apply to the focused tab.
    return;
  }
  auto* observer = skills::SkillsUpdateObserver::From(focused_tab);
  if (!observer) {
    return;
  }
  host_->NotifyContextualSkillsChanged(observer->GetContextualSkills());
}

void GlicSkillsManagerImpl::OnFocusedTabChanged(
    const FocusedTabData& focused_tab_data) {
  UpdateSkillPreviews(std::nullopt);
}

void GlicSkillsManagerImpl::WebUiStateChanged(mojom::WebUiState state) {
  if (state == mojom::WebUiState::kReady) {
    UpdateSkillPreviews(std::nullopt);
  }
}

}  // namespace glic
