// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/web_contents_can_go_back_observer.h"

#include "base/memory/ptr_util.h"
#include "content/public/browser/navigation_details.h"
#include "ui/aura/window.h"
#include "ui/platform_window/extensions/wayland_extension.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_lacros.h"
#include "ui/views/widget/widget.h"

WebContentsCanGoBackObserver::WebContentsCanGoBackObserver(
    content::WebContents* web_contents)
    : content::WebContentsUserData<WebContentsCanGoBackObserver>(*web_contents),
      content::WebContentsObserver(web_contents) {}

WebContentsCanGoBackObserver::~WebContentsCanGoBackObserver() = default;

// static
WebContentsCanGoBackObserver*
WebContentsCanGoBackObserver::CreateForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);

  WebContentsCanGoBackObserver* observer = FromWebContents(web_contents);
  if (!observer) {
    observer = new WebContentsCanGoBackObserver(web_contents);
    web_contents->SetUserData(UserDataKey(), base::WrapUnique(observer));
  }
  return observer;
}

void WebContentsCanGoBackObserver::OnVisibilityChanged(
    content::Visibility visibility) {
  // The visibility change hook takes place when a tab changes its visibility
  // state (eg the tab switching). In the case a tab isn't visible, it can not
  // interfere on actions that relate to "can go back" state.
  visible_ = visibility == content::Visibility::VISIBLE;

  UpdateLatestFocusedWebContentsStatus();
}

void WebContentsCanGoBackObserver::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  UpdateLatestFocusedWebContentsStatus();
}

void WebContentsCanGoBackObserver::UpdateLatestFocusedWebContentsStatus() {
  if (!visible_)
    return;

  aura::Window* window = web_contents()->GetNativeView();
  if (!window->GetHost())
    return;

  // Lacros is based on Ozone/Wayland which uses DesktopWindowTreeHostLacros.
  auto* dwth_platform =
      views::DesktopWindowTreeHostLacros::From(window->GetHost());
  if (!dwth_platform)
    return;

  bool can_go_back = web_contents()->GetController().CanGoBack();
  auto* wayland_extension = dwth_platform->GetWaylandToplevelExtension();
  wayland_extension->SetCanGoBack(can_go_back);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebContentsCanGoBackObserver);
