// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_group_sync/tab_group_sync_tab_state.h"

#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/saved_tab_groups/public/features.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace {
void SetShouldDeferMediaLoadOnRenderer(content::WebContents* web_contents,
                                       bool should_defer) {
  if (tab_groups::DeferMediaLoadInBackgroundTab()) {
    content::RenderFrameHost* render_frame_host =
        web_contents->GetPrimaryMainFrame();
    mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
        chrome_render_frame;
    render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
        &chrome_render_frame);
    chrome_render_frame->SetShouldDeferMediaLoad(should_defer);
  }
}

}  // namespace

TabGroupSyncTabState::TabGroupSyncTabState(content::WebContents* web_contents)
    : content::WebContentsUserData<TabGroupSyncTabState>(*web_contents) {}

TabGroupSyncTabState::~TabGroupSyncTabState() = default;

void TabGroupSyncTabState::Create(content::WebContents* web_contents) {
  SetShouldDeferMediaLoadOnRenderer(web_contents, /*should_defer*/ true);
  if (!TabGroupSyncTabState::FromWebContents(web_contents)) {
    TabGroupSyncTabState::CreateForWebContents(web_contents);
  }
}

void TabGroupSyncTabState::Reset(content::WebContents* web_contents) {
  web_contents->RemoveUserData(TabGroupSyncTabState::UserDataKey());
  SetShouldDeferMediaLoadOnRenderer(web_contents, /*should_defer*/ false);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(TabGroupSyncTabState);
