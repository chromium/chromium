// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/passwords_navigation_observer.h"

#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

PasswordsNavigationObserver::PasswordsNavigationObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  if (auto* client =
          ChromePasswordManagerClient::FromWebContents(web_contents)) {
    password_manager_observation_.Observe(client->GetPasswordManager());
  }
}
PasswordsNavigationObserver::~PasswordsNavigationObserver() = default;

void PasswordsNavigationObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted()) {
    return;
  }

  if (quit_on_entry_committed_) {
    waiter_helper_.OnEvent();
  }
}

void PasswordsNavigationObserver::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  bool is_target_navigation = false;
  if (!wait_for_path_.empty()) {
    if (validated_url.GetPath() == wait_for_path_) {
      is_target_navigation = true;
    }
  } else if (!render_frame_host->GetParent()) {
    is_target_navigation = true;
  }

  if (!is_target_navigation) {
    return;
  }

  if (wait_for_password_forms_parsed_ && !password_forms_parsed_) {
    wait_for_forms_after_load_ = true;
    return;
  }

  waiter_helper_.OnEvent();
}

void PasswordsNavigationObserver::OnPasswordFormsParsed(
    password_manager::PasswordManagerDriver* driver,
    const std::vector<autofill::FormData>& forms_data) {
  if (!wait_for_password_forms_parsed_) {
    return;
  }
  password_forms_parsed_ = true;
  if (wait_for_forms_after_load_) {
    wait_for_forms_after_load_ = false;
    waiter_helper_.OnEvent();
  }
}

bool PasswordsNavigationObserver::Wait() {
  return waiter_helper_.Wait();
}
