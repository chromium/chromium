// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/models/menu_model.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#endif

using ui::MenuModel;

TestRenderViewContextMenu::TestRenderViewContextMenu(
    content::RenderFrameHost& render_frame_host,
    content::ContextMenuParams params)
    : RenderViewContextMenu(render_frame_host, params) {}

TestRenderViewContextMenu::~TestRenderViewContextMenu() {}

// static
std::unique_ptr<TestRenderViewContextMenu> TestRenderViewContextMenu::Create(
    content::WebContents* web_contents,
    const GURL& frame_url,
    const GURL& link_url,
    bool is_subframe) {
  return Create(web_contents->GetPrimaryMainFrame(), frame_url, link_url,
                is_subframe);
}

// static
std::unique_ptr<TestRenderViewContextMenu> TestRenderViewContextMenu::Create(
    content::RenderFrameHost* render_frame_host,
    const GURL& frame_url,
    const GURL& link_url,
    bool is_subframe) {
  content::ContextMenuParams params;
  params.page_url = frame_url;
  params.frame_url = frame_url;
  params.link_url = link_url;
  params.is_subframe = is_subframe;
  auto menu =
      std::make_unique<TestRenderViewContextMenu>(*render_frame_host, params);
  menu->Init();
  return menu;
}

bool TestRenderViewContextMenu::IsItemPresent(int command_id) const {
  return menu_model_.GetIndexOfCommandId(command_id).has_value();
}

bool TestRenderViewContextMenu::IsItemChecked(int command_id) const {
  const std::optional<size_t> index =
      menu_model_.GetIndexOfCommandId(command_id);
  return index && menu_model_.IsItemCheckedAt(*index);
}

bool TestRenderViewContextMenu::IsItemEnabled(int command_id) const {
  const std::optional<size_t> index =
      menu_model_.GetIndexOfCommandId(command_id);
  return index && menu_model_.IsEnabledAt(*index);
}

bool TestRenderViewContextMenu::IsItemInRangePresent(
    int command_id_first,
    int command_id_last) const {
  DCHECK_LE(command_id_first, command_id_last);
  for (int command_id = command_id_first; command_id <= command_id_last;
       ++command_id) {
    if (IsItemPresent(command_id))
      return true;
  }
  return false;
}

bool TestRenderViewContextMenu::GetMenuModelAndItemIndex(
    int command_id,
    raw_ptr<MenuModel>* found_model,
    size_t* found_index) {
  std::vector<MenuModel*> models_to_search;
  models_to_search.push_back(&menu_model_);

  while (!models_to_search.empty()) {
    MenuModel* model = models_to_search.back();
    models_to_search.pop_back();
    for (size_t i = 0; i < model->GetItemCount(); i++) {
      if (model->GetCommandIdAt(i) == command_id) {
        *found_model = model;
        *found_index = i;
        return true;
      }
      if (model->GetTypeAt(i) == MenuModel::TYPE_SUBMENU) {
        models_to_search.push_back(model->GetSubmenuModelAt(i));
      }
    }
  }

  return false;
}

int TestRenderViewContextMenu::GetCommandIDByProfilePath(
    const base::FilePath& path) const {
  size_t count = profile_link_paths_.size();
  for (size_t i = 0; i < count; ++i) {
    if (profile_link_paths_[i] == path)
      return IDC_OPEN_LINK_IN_PROFILE_FIRST + static_cast<int>(i);
  }
  return -1;
}

void TestRenderViewContextMenu::SetBrowser(Browser* browser) {
  browser_ = browser;
}

Browser* TestRenderViewContextMenu::GetBrowser() const {
  if (browser_)
    return browser_;
  return RenderViewContextMenu::GetBrowser();
}

void TestRenderViewContextMenu::Show() {
}

#if BUILDFLAG(IS_CHROMEOS)
const policy::DlpRulesManager* TestRenderViewContextMenu::GetDlpRulesManager()
    const {
  return dlp_rules_manager_;
}

void TestRenderViewContextMenu::set_dlp_rules_manager(
    policy::DlpRulesManager* dlp_rules_manager) {
  dlp_rules_manager_ = dlp_rules_manager;
}
#endif

#if BUILDFLAG(ENABLE_COMPOSE)
ChromeComposeClient* TestRenderViewContextMenu::GetChromeComposeClient() const {
  return compose_client_;
}

void TestRenderViewContextMenu::SetChromeComposeClient(
    ChromeComposeClient* compose_client) {
  compose_client_ = compose_client;
}
#endif  // BUILDFLAG(ENABLE_COMPOSE)
