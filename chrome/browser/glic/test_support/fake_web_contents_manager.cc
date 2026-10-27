// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/test_support/fake_web_contents_manager.h"

#include "content/public/browser/web_contents.h"

namespace glic {

FakeWebContentsManager::FakeWebContentsManager(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

FakeWebContentsManager::~FakeWebContentsManager() = default;

void FakeWebContentsManager::AttachToHost(Host* host) {
  host_ = host;
}

void FakeWebContentsManager::AttachModalDialogManagerDelegate(
    ScopedModalDialogManagerDelegate& delegate) {}

void FakeWebContentsManager::SetVisibility(content::Visibility visibility) {
  visibility_ = visibility;
}

content::WebContents* FakeWebContentsManager::active_web_contents() const {
  return web_contents_;
}

content::WebContents* FakeWebContentsManager::guest_contents() const {
  return web_client_manager_.web_client_contents();
}

void FakeWebContentsManager::OnActuatingChanged(bool actuating) {
  is_actuating_ = actuating;
}

void FakeWebContentsManager::OnTaskTabsVisibilityChanged(bool has_visible_tab) {
  has_visible_task_tabs_ = has_visible_tab;
}

base::CallbackListSubscription
FakeWebContentsManager::RegisterWebContentsChangedCallback(
    WebContentsChangedCallback callback) {
  return web_contents_changed_callbacks_.Add(std::move(callback));
}

GlicWebClientManager& FakeWebContentsManager::web_client_manager() {
  return web_client_manager_;
}

bool FakeWebContentsManager::ShouldReloadOnShow() const {
  return should_reload_on_show_ ||
         (web_contents_ ? web_contents_->IsCrashed() : false);
}

bool FakeWebContentsManager::IsCrashed() const {
  return web_contents_ ? web_contents_->IsCrashed() : false;
}

void FakeWebContentsManager::SetErrorCallback(base::RepeatingClosure callback) {
  error_callback_ = std::move(callback);
}

void FakeWebContentsManager::TriggerErrorCallback() {
  if (error_callback_) {
    error_callback_.Run();
  }
}

}  // namespace glic
