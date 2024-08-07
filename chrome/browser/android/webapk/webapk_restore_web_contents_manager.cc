// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_restore_web_contents_manager.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/chrome_security_state_tab_helper.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

namespace webapk {

WebApkRestoreWebContentsManager::WebApkRestoreWebContentsManager(
    Profile* profile)
    : profile_(profile->GetWeakPtr()) {}
WebApkRestoreWebContentsManager::~WebApkRestoreWebContentsManager() = default;

void WebApkRestoreWebContentsManager::EnsureWebContentsCreated(
    base::PassKey<WebApkRestoreManager> pass_key) {
  CHECK(profile_);
  if (!shared_web_contents_) {
    shared_web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_.get()));

    // Create WebContents dependencies.
    webapps::InstallableManager::CreateForWebContents(
        shared_web_contents_.get());
    ChromeSecurityStateTabHelper::CreateForWebContents(
        shared_web_contents_.get());
  }
}

void WebApkRestoreWebContentsManager::ClearSharedWebContents() {
  url_loader_.reset();
  shared_web_contents_.reset();
}

base::WeakPtr<WebApkRestoreWebContentsManager>
WebApkRestoreWebContentsManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

content::WebContents* WebApkRestoreWebContentsManager::web_contents() {
  return shared_web_contents_.get();
}

void WebApkRestoreWebContentsManager::LoadUrl(
    const GURL& url,
    webapps::WebAppUrlLoader::ResultCallback result_callback) {
  if (!url_loader_) {
    url_loader_ = std::make_unique<webapps::WebAppUrlLoader>();
  }
  url_loader_->LoadUrl(
      url, web_contents(),
      webapps::WebAppUrlLoader::UrlComparison::kIgnoreQueryParamsAndRef,
      std::move(result_callback));
}

}  // namespace webapk
