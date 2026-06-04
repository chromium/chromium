// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_skills_manager_impl.h"

#include "base/functional/bind.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/skills/skills_dialog_launcher.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/browser/skills/skills_update_observer.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/common/webui_url_constants.h"
#include "components/skills/features.h"
#include "components/skills/public/skills_service.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/base_window.h"

namespace glic {

GlicSkillsManagerImpl::GlicSkillsManagerImpl(GlicInstance* instance,
                                             Profile* profile)
    : instance_(*instance), profile_(*profile) {
  focused_tab_changed_subscription_ =
      instance->host().sharing_manager().AddFocusedTabChangedCallback(
          base::BindRepeating(&GlicSkillsManagerImpl::OnFocusedTabChanged,
                              weak_ptr_factory_.GetWeakPtr()));
  host_observation_.Observe(&instance->host());
}

GlicSkillsManagerImpl::~GlicSkillsManagerImpl() = default;

void GlicSkillsManagerImpl::UpdateSkillPreviews(
    std::optional<tabs::TabInterface*> updated_tab) {
  if (!instance_->host().IsWebClientConnected()) {
    return;
  }
  auto* focused_tab =
      instance_->host().sharing_manager().GetFocusedTabData().focus();
  if (!focused_tab) {
    instance_->host().NotifyContextualSkillsChanged({});
    contextual_skill_previews_.clear();
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
  auto new_skill_previews = observer->GetContextualSkillPreviews();

  if (mojo::Equals(contextual_skill_previews_, new_skill_previews)) {
    return;
  }
  contextual_skill_previews_ = std::move(new_skill_previews);

  std::vector<mojom::SkillPreviewPtr> skill_previews;
  for (const auto& preview : contextual_skill_previews_) {
    skill_previews.push_back(preview.Clone());
  }
  instance_->host().NotifyContextualSkillsChanged(std::move(skill_previews));
}

tabs::TabInterface* GlicSkillsManagerImpl::EnsureTabForSkills() {
  const FocusedTabData& ftd =
      instance_->host().sharing_manager().GetFocusedTabData();
  tabs::TabInterface* tab = ftd.focus() ? ftd.focus() : ftd.unfocused_tab();

  if (tab) {
    return tab;
  }

  content::WebContents* guest_contents =
      instance_->host().web_client_contents();
  if (!guest_contents) {
    return nullptr;
  }

  BrowserWindowInterface* active_browser = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&active_browser, this](BrowserWindowInterface* browser) {
        if (browser->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL &&
            browser->GetProfile() == &*profile_) {
          active_browser = browser;
          return false;
        }
        return true;
      });

  if (!active_browser) {
    return nullptr;
  }

  return TabListInterface::From(active_browser)
      ->OpenTab(GURL("chrome://newtab"), -1);
}

void GlicSkillsManagerImpl::LaunchSkillsDialog(
    Profile* profile,
    skills::Skill skill,
    skills::mojom::SkillsDialogType dialog_type,
    base::OnceCallback<void(bool)> callback) {
  tabs::TabInterface* target_tab = EnsureTabForSkills();

  if (!target_tab) {
    std::move(callback).Run(false);
    return;
  }
  // Delegate the race-condition handling to the Skills launcher.
  auto target = std::make_unique<glic::Target>();
  target->surface = target_tab;
  if (auto conv_id = instance_->conversation_id()) {
    target->conversation = glic::ConversationId(*conv_id);
  }
  skills::SkillsDialogLauncher::CreateForTab(target_tab, std::move(skill),
                                             dialog_type, std::move(target),
                                             std::move(callback));
}

void GlicSkillsManagerImpl::ShowManageSkillsUi() {
  ShowSkillsUiAtRelativePath(chrome::kChromeUISkillsYourSkillsPath);
}

void GlicSkillsManagerImpl::ShowBrowseSkillsUi() {
  ShowSkillsUiAtRelativePath(chrome::kChromeUISkillsBrowsePath);
}

void GlicSkillsManagerImpl::ShowSkillsUiAtRelativePath(
    const std::string& path) {
  const GURL skills_url = GURL(chrome::kChromeUISkillsURL).Resolve(path);
  bool existing_skills_tab_found = false;

  BrowserWindowInterface* most_recent_browser = nullptr;

  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&skills_url, &existing_skills_tab_found, &most_recent_browser,
       this](BrowserWindowInterface* browser) {
        if (browser->GetType() != BrowserWindowInterface::Type::TYPE_NORMAL ||
            browser->GetProfile() != &*profile_) {
          return true;
        }

        if (!most_recent_browser) {
          most_recent_browser = browser;
        }

        TabListInterface* tab_list = TabListInterface::From(browser);
        if (!tab_list) {
          return true;
        }
        for (const auto& tab : tab_list->GetAllTabs()) {
          content::WebContents* web_contents = tab->GetContents();
          if (web_contents && web_contents->GetURL() == skills_url) {
            if (browser->GetWindow()) {
              browser->GetWindow()->Activate();
            }
            tab_list->ActivateTab(tab->GetHandle());
            existing_skills_tab_found = true;
            return false;
          }
        }
        return true;
      });

  if (!existing_skills_tab_found) {
    if (most_recent_browser) {
      TabListInterface::From(most_recent_browser)
          ->OpenTab(skills_url, /*index=*/-1);
      return;
    }

    BrowserWindowCreateParams create_params(
        BrowserWindowInterface::Type::TYPE_NORMAL, *profile_,
        /*from_user_gesture=*/true);

    CreateBrowserWindow(
        std::move(create_params),
        base::BindOnce(
            [](const GURL& url, BrowserWindowInterface* browser_window) {
              if (browser_window) {
                if (auto* tab_list = TabListInterface::From(browser_window)) {
                  tab_list->OpenTab(url, /*index=*/-1);
                }
              }
            },
            skills_url));
  }
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

void GlicSkillsManagerImpl::NotifyPanelOpenedOrActivated() {
  // NEEDS_ANDROID_IMPL: (crbug.com/477622144) Remove desktop-only
  // restrictions from Skills backend.
#if !BUILDFLAG(IS_ANDROID)  // NEEDS_ANDROID_IMPL
  if (base::FeatureList::IsEnabled(features::kSkillsEnabled)) {
    skills::SkillsServiceFactory::GetForProfile(&*profile_)
        ->RefreshDiscoverySkills();
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

}  // namespace glic
