// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/sharesheet/nearby_share_web_view.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"

NearbyShareWebView::NearbyShareWebView(content::BrowserContext* browser_context)
    : WebView(browser_context) {}

void NearbyShareWebView::WebContentsCreated(
    content::WebContents* source_contents,
    int opener_render_process_id,
    int opener_render_frame_id,
    const std::string& frame_name,
    const GURL& target_url,
    content::WebContents* new_contents) {
  chrome::ScopedTabbedBrowserDisplayer displayer(
      Profile::FromBrowserContext(GetBrowserContext()));
  NavigateParams nav_params(displayer.browser(), target_url,
                            ui::PageTransition::PAGE_TRANSITION_LINK);
  Navigate(&nav_params);
}
