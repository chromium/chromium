// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager_lacros.h"

#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"

namespace policy {

namespace {
static DlpContentManagerLacros* g_dlp_content_manager = nullptr;
}  // namespace

// static
DlpContentManagerLacros* DlpContentManagerLacros::Get() {
  if (!g_dlp_content_manager) {
    g_dlp_content_manager = new DlpContentManagerLacros();
  }
  return g_dlp_content_manager;
}

DlpContentManagerLacros::DlpContentManagerLacros() = default;
DlpContentManagerLacros::~DlpContentManagerLacros() = default;

void DlpContentManagerLacros::OnConfidentialityChanged(
    content::WebContents* web_contents,
    const DlpContentRestrictionSet& restriction_set) {
  confidential_web_contents_[web_contents] = restriction_set;
  aura::Window* window = web_contents->GetNativeView();
  if (!window_webcontents_.contains(window)) {
    window_webcontents_[window] = {};
    window->AddObserver(this);
  }
  window_webcontents_[window].insert(web_contents);
  UpdateRestrictions(window);
}

void DlpContentManagerLacros::OnWebContentsDestroyed(
    content::WebContents* web_contents) {
  confidential_web_contents_.erase(web_contents);
  aura::Window* window = web_contents->GetNativeView();
  if (window_webcontents_.contains(window)) {
    window_webcontents_[window].erase(web_contents);
    UpdateRestrictions(window);
  }
}

void DlpContentManagerLacros::OnVisibilityChanged(
    content::WebContents* web_contents) {
  aura::Window* window = web_contents->GetNativeView();
  UpdateRestrictions(window);
}

void DlpContentManagerLacros::OnWindowDestroying(aura::Window* window) {
  window->RemoveObserver(this);
  window_webcontents_.erase(window);
  confidential_windows_.erase(window);
}

void DlpContentManagerLacros::UpdateRestrictions(aura::Window* window) {
  DlpContentRestrictionSet new_restrictions;
  for (auto* web_contents : window_webcontents_[window]) {
    if (web_contents->GetVisibility() == content::Visibility::VISIBLE) {
      new_restrictions.UnionWith(confidential_web_contents_[web_contents]);
    }
  }
  if (new_restrictions != confidential_windows_[window]) {
    confidential_windows_[window] = new_restrictions;
    // TODO(crbug.com/1260467): Notify Ash.
  }
}

}  // namespace policy
