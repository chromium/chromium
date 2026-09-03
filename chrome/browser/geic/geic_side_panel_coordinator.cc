// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geic/geic_side_panel_coordinator.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/geic/geic_pwc_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_scope.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/unowned_user_data/user_data_factory.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view.h"

namespace geic {

DEFINE_USER_DATA(GeicSidePanelCoordinator);

// static
GeicSidePanelCoordinator* GeicSidePanelCoordinator::From(
    tabs::TabInterface* tab) {
  return tab ? GeicSidePanelCoordinator::Get(tab->GetUnownedUserDataHost())
             : nullptr;
}

GeicSidePanelCoordinator::GeicSidePanelCoordinator(
    tabs::TabInterface& tab_interface,
    SidePanelRegistry* registry)
    : tab_interface_(tab_interface),
      scoped_unowned_user_data_(tab_interface.GetUnownedUserDataHost(), *this) {
  if (registry) {
    CreateAndRegisterEntry(registry);
  }
}

GeicSidePanelCoordinator::~GeicSidePanelCoordinator() = default;

void GeicSidePanelCoordinator::CreateAndRegisterEntry(
    SidePanelRegistry* registry) {
  auto entry = std::make_unique<SidePanelEntry>(
      SidePanelEntryKey(SidePanelEntryId::kGeic),
      base::BindRepeating(&GeicSidePanelCoordinator::CreateGeicView,
                          base::Unretained(this)),
      /*default_content_width_callback=*/base::NullCallback());
  entry->set_should_show_header(false);
  registry->Register(std::move(entry));
}

void GeicSidePanelCoordinator::Toggle() {
  if (auto* side_panel_ui =
          SidePanelUI::From(tab_interface_->GetBrowserWindowInterface())) {
    // TODO(crbug.com/545285265): Handle case when a non-GEiC side panel is
    // showing.
    if (side_panel_ui->IsSidePanelShowing()) {
      side_panel_ui->Close();
    } else {
      side_panel_ui->Show(SidePanelEntryId::kGeic);
    }
  }
}

std::unique_ptr<views::View> GeicSidePanelCoordinator::CreateGeicView(
    SidePanelEntryScope& scope) {
  auto* profile = tab_interface_->GetProfile();
  auto* geic_manager = GeicPwcManager::GetOrCreateForProfile(profile);
  tabs::TabInterface* tab = &tab_interface_.get();
  content::WebContents* web_contents =
      geic_manager ? geic_manager->GetOrCreateWebContentsForTab(tab) : nullptr;
  auto web_view = std::make_unique<views::WebView>(profile);
  if (web_contents) {
    web_view->SetWebContents(web_contents);
  }
  return web_view;
}

}  // namespace geic
