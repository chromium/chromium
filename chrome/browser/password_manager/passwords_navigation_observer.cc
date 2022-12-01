// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/passwords_navigation_observer.h"

#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

PasswordsNavigationObserver::PasswordsNavigationObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}
PasswordsNavigationObserver::~PasswordsNavigationObserver() = default;

void PasswordsNavigationObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted())
    return;

  if (quit_on_entry_committed_)
    run_loop_.Quit();
}

void PasswordsNavigationObserver::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (!wait_for_path_.empty()) {
    if (validated_url.path() == wait_for_path_)
      run_loop_.Quit();
  } else if (!render_frame_host->GetParent()) {
    run_loop_.Quit();
  }
}

void PasswordsNavigationObserver::Wait() {
  run_loop_.Run();
}
