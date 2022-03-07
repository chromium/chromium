// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/desk_template_client_lacros.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
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
