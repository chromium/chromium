// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_skills_manager_impl.h"

#include "base/functional/bind.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/skills/skills_dialog_launcher.h"
#include "chrome/browser/skills/skills_update_observer.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/common/webui_url_constants.h"
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

tabs::TabInterface* GlicSkillsManagerImpl::EnsureTabForSkills() {
  const FocusedTabData& ftd = host_->sharing_manager().GetFocusedTabData();
  tabs::TabInterface* tab = ftd.focus() ? ftd.focus() : ftd.unfocused_tab();

  if (tab) {
    return tab;
  }

  content::WebContents* guest_contents = host_->web_client_contents();
  if (!guest_contents) {
    return nullptr;
  }

  Profile* profile =
      Profile::FromBrowserContext(guest_contents->GetBrowserContext());
  chrome::ScopedTabbedBrowserDisplayer displayer(profile);
  if (!displayer.browser()) {
    return nullptr;
  }

  content::WebContents* contents = chrome::AddAndReturnTabAt(
      displayer.browser(), GURL("chrome://newtab"), -1, true);

  return tabs::TabInterface::MaybeGetFromContents(contents);
}

void GlicSkillsManagerImpl::LaunchSkillsDialog(
    Profile* profile,
    skills::Skill skill,
    base::OnceCallback<void(bool)> callback) {
  tabs::TabInterface* target_tab = EnsureTabForSkills();

  if (!target_tab) {
    std::move(callback).Run(false);
    return;
  }
  // Delegate the race-condition handling to the Skills launcher.
  skills::SkillsDialogLauncher::CreateForTab(target_tab, std::move(skill),
                                             std::move(callback));
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
