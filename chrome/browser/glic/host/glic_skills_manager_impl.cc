// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_skills_manager_impl.h"

#include "base/functional/bind.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/skills/skills_dialog_launcher.h"
#include "chrome/browser/skills/skills_update_observer.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/common/webui_url_constants.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/base_window.h"
#include "chrome/browser/ui/browser_tabstrip.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/skills/skills_dialog_launcher.h"
#include "chrome/browser/skills/skills_update_observer.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif  // !BUILDFLAG(IS_ANDROID)

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
#if BUILDFLAG(IS_ANDROID)
  return;
#else
  if (!host_->IsReady()) {
    return;
  }
  auto* focused_tab = host_->sharing_manager().GetFocusedTabData().focus();
  if (!focused_tab) {
    host_->NotifyContextualSkillsChanged({});
    contextual_skills_.clear();
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
  auto new_contextual_skills = observer->GetContextualSkills();
  if (mojo::Equals(contextual_skills_, new_contextual_skills)) {
    return;
  }
  contextual_skills_ = std::move(new_contextual_skills);

  std::vector<mojom::SkillPreviewPtr> skill_previews;
  for (const auto& skill : contextual_skills_) {
    skill_previews.push_back(skill->preview.Clone());
  }
  host_->NotifyContextualSkillsChanged(std::move(skill_previews));
#endif
}

tabs::TabInterface* GlicSkillsManagerImpl::EnsureTabForSkills() {
#if BUILDFLAG(IS_ANDROID)
  return nullptr;
#else
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
#endif
}

void GlicSkillsManagerImpl::LaunchSkillsDialog(
    Profile* profile,
    skills::Skill skill,
    base::OnceCallback<void(bool)> callback) {
#if BUILDFLAG(IS_ANDROID)
  return;
#else
  tabs::TabInterface* target_tab = EnsureTabForSkills();

  if (!target_tab) {
    std::move(callback).Run(false);
    return;
  }
  // Delegate the race-condition handling to the Skills launcher.
  skills::SkillsDialogLauncher::CreateForTab(target_tab, std::move(skill),
                                             std::move(callback));
#endif
}

void GlicSkillsManagerImpl::ShowManageSkillsUi() {
#if !BUILDFLAG(IS_ANDROID)
  const GURL skills_url = GURL(chrome::kChromeUISkillsURL)
                              .Resolve(chrome::kChromeUISkillsYourSkillsPath);
  bool existing_skills_tab_found = false;

  Profile* host_profile = host_->profile();
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&skills_url, &existing_skills_tab_found,
       host_profile](BrowserWindowInterface* browser) {
        if (browser->GetType() != BrowserWindowInterface::Type::TYPE_NORMAL ||
            browser->GetProfile() != host_profile) {
          return true;
        }
        TabStripModel* tab_strip = browser->GetTabStripModel();
        for (int i = 0; i < tab_strip->count(); ++i) {
          content::WebContents* web_contents = tab_strip->GetWebContentsAt(i);
          if (web_contents && web_contents->GetURL() == skills_url) {
            browser->GetWindow()->Activate();
            tab_strip->ActivateTabAt(i);
            existing_skills_tab_found = true;
            return false;
          }
        }
        return true;
      });

  if (!existing_skills_tab_found) {
    NavigateParams params(host_->profile(), skills_url,
                          ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
    params.disposition = WindowOpenDisposition::SINGLETON_TAB;
    Navigate(&params);
  }
#endif  // !BUILDFLAG(IS_ANDROID)
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

glic::mojom::SkillPtr GlicSkillsManagerImpl::GetContextualSkill(
    std::string_view skill_id) {
  for (const auto& skill : contextual_skills_) {
    if (skill->preview->id == skill_id) {
      return skill.Clone();
    }
  }
  return nullptr;
}

}  // namespace glic
