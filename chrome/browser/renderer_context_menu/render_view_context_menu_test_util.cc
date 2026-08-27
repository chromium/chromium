// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/indigo/indigo_image_replacement.h"
#include "chrome/browser/indigo/indigo_image_replacement_manager.h"
#include "chrome/browser/renderer_context_menu/context_menu_test_util.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/models/menu_model.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#endif

#if BUILDFLAG(ENABLE_COMPOSE)
#include "chrome/browser/compose/chrome_compose_client.h"
#endif

TestRenderViewContextMenu::TestRenderViewContextMenu(
    content::RenderFrameHost& render_frame_host,
    content::ContextMenuParams params)
    : RenderViewContextMenu(render_frame_host,
                            params,
                            /*is_paste_enabled=*/false,
                            /*is_paste_and_match_style_enabled=*/false) {}

TestRenderViewContextMenu::~TestRenderViewContextMenu() = default;

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

std::optional<std::pair<ui::MenuModel*, size_t>>
TestRenderViewContextMenu::GetMenuModelAndItemIndex(int command_id) {
  return context_menu_test_util::GetMenuModelAndItemIndex(&menu_model_,
                                                          command_id);
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

void TestRenderViewContextMenu::SetBrowser(BrowserWindowInterface* browser) {
  browser_ = browser;
}

BrowserWindowInterface* TestRenderViewContextMenu::GetBrowser() const {
  if (browser_) {
    return browser_;
  }
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

GURL TestRenderViewContextMenu::GetIndigoReplacementImageURL() const {
  GURL url = RenderViewContextMenu::GetIndigoReplacementImageURL();
  if (!url.is_empty()) {
    return url;
  }
  // In tests, `params_.image_replacement_frame_token` may be populated directly
  // from a child RenderFrameHost's LocalFrameToken across process boundaries
  // rather than from a placeholder RemoteFrameToken in the parent process.
  if (!params_.image_replacement_frame_token.has_value() ||
      !params_.image_replacement_frame_token->Is<blink::LocalFrameToken>()) {
    return GURL();
  }
  content::RenderFrameHost* frame_host = GetRenderFrameHost();
  if (!frame_host) {
    return GURL();
  }
  content::RenderFrameHost* subframe_host = nullptr;
  if (content::WebContents* web_contents =
          content::WebContents::FromRenderFrameHost(frame_host)) {
    web_contents->ForEachRenderFrameHost([&](content::RenderFrameHost* rfh) {
      if (rfh->GetFrameToken() == params_.image_replacement_frame_token
                                      ->GetAs<blink::LocalFrameToken>()) {
        subframe_host = rfh;
      }
    });
  }
  if (!subframe_host || &subframe_host->GetPage() != &frame_host->GetPage() ||
      subframe_host->GetParent() != frame_host) {
    return GURL();
  }
  auto* manager =
      indigo::IndigoImageReplacementManager::GetForPage(frame_host->GetPage());
  if (!manager) {
    return GURL();
  }
  auto* replacement = manager->GetImageReplacementForFrame(*subframe_host);
  return replacement ? replacement->GetReplacementImageURL() : GURL();
}
