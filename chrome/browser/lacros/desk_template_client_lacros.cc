// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/desk_template_client_lacros.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chromeos/lacros/lacros_service.h"
#include "ui/platform_window/platform_window.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"

DeskTemplateClientLacros::DeskTemplateClientLacros() {
  auto* const lacros_service = chromeos::LacrosService::Get();
  if (lacros_service->IsAvailable<crosapi::mojom::DeskTemplate>()) {
    lacros_service->GetRemote<crosapi::mojom::DeskTemplate>()
        ->AddDeskTemplateClient(receiver_.BindNewPipeAndPassRemote());
  }
}

DeskTemplateClientLacros::~DeskTemplateClientLacros() = default;

void DeskTemplateClientLacros::CreateBrowserWithRestoredData(
    const gfx::Rect& current_bounds,
    const ui::mojom::WindowShowState window_show_state,
    crosapi::mojom::DeskTemplateStatePtr tabstrip_state) {
  Profile* profile = ProfileManager::GetLastUsedProfileAllowedByPolicy();
  DCHECK(profile) << "No last used profile is found.";
  Browser::CreateParams create_params = Browser::CreateParams(
      Browser::TYPE_NORMAL, profile, /*user_gesture=*/false);
  create_params.initial_show_state =
      static_cast<ui::WindowShowState>(window_show_state);
  create_params.initial_bounds = current_bounds;
  Browser* browser = Browser::Create(create_params);
  for (size_t i = 0; i < tabstrip_state->urls.size(); i++) {
    chrome::AddTabAt(browser, tabstrip_state->urls.at(i), /*index=*/-1,
                     /*foreground=*/
                     (i == static_cast<size_t>(tabstrip_state->active_index)));
  }
  if (window_show_state == ui::mojom::WindowShowState::SHOW_STATE_MINIMIZED) {
    browser->window()->Minimize();
  } else {
    browser->window()->ShowInactive();
  }
}

void DeskTemplateClientLacros::GetTabStripModelUrls(
    uint32_t serial,
    const std::string& window_unique_id,
    GetTabStripModelUrlsCallback callback) {
  Browser* browser = nullptr;
  for (auto* b : *BrowserList::GetInstance()) {
    if (views::DesktopWindowTreeHostLinux::From(
            b->window()->GetNativeWindow()->GetHost())
            ->platform_window()
            ->GetWindowUniqueId() == window_unique_id) {
      browser = b;
      break;
    }
  }

  if (!browser) {
    std::move(callback).Run(serial, window_unique_id, {});
    return;
  }

  crosapi::mojom::DeskTemplateStatePtr state =
      crosapi::mojom::DeskTemplateState::New();
  TabStripModel* tab_strip_model = browser->tab_strip_model();
  state->active_index = tab_strip_model->active_index();
  for (int i = 0; i < tab_strip_model->count(); ++i) {
    state->urls.push_back(
        tab_strip_model->GetWebContentsAt(i)->GetLastCommittedURL());
  }
  std::move(callback).Run(serial, window_unique_id, std::move(state));
}
