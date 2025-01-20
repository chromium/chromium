// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/chrome_password_change_service.h"

#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/password_manager/password_change_delegate_impl.h"
#include "components/affiliations/core/browser/affiliation_service.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace {

content::WebContents* OpenNewTab(const GURL& url,
                                 content::WebContents* original_tab) {
  CHECK(original_tab);
  return original_tab->OpenURL(
      content::OpenURLParams(url, content::Referrer(),
                             WindowOpenDisposition::NEW_BACKGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK,
                             /*is_renderer_initiated=*/false),
      base::DoNothing());
}

}  // namespace

ChromePasswordChangeService::ChromePasswordChangeService(
    affiliations::AffiliationService* affiliation_service)
    : affiliation_service_(affiliation_service),
      new_tab_callback_(base::BindRepeating(&OpenNewTab)) {}

ChromePasswordChangeService::~ChromePasswordChangeService() {
  for (const auto& delegate : password_change_delegates_) {
    delegate->RemoveObserver(this);
  }
}

bool ChromePasswordChangeService::IsPasswordChangeSupported(const GURL& url) {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kImprovedPasswordChangeService)) {
    return false;
  }

  return affiliation_service_->GetChangePasswordURL(url).is_valid();
}

void ChromePasswordChangeService::StartPasswordChange(
    const GURL& url,
    const std::u16string& username,
    const std::u16string& password,
    content::WebContents* web_contents) {
  GURL change_pwd_url = affiliation_service_->GetChangePasswordURL(url);
  CHECK(change_pwd_url.is_valid());

  std::unique_ptr<PasswordChangeDelegate> controller =
      std::make_unique<PasswordChangeDelegateImpl>(
          std::move(change_pwd_url), username, password, web_contents,
          new_tab_callback_);
  controller->AddObserver(this);
  password_change_delegates_.push_back(std::move(controller));
}

PasswordChangeDelegate* ChromePasswordChangeService::GetPasswordChangeDelegate(
    content::WebContents* web_contents) {
  for (const auto& delegate : password_change_delegates_) {
    if (delegate->IsPasswordChangeOngoing(web_contents)) {
      return delegate.get();
    }
  }
  return nullptr;
}

void ChromePasswordChangeService::OnPasswordChangeStopped(
    PasswordChangeDelegate* delegate) {
  delegate->RemoveObserver(this);

  auto iter = base::ranges::find(password_change_delegates_, delegate,
                                 &std::unique_ptr<PasswordChangeDelegate>::get);
  CHECK(iter != password_change_delegates_.end());

  std::unique_ptr<PasswordChangeDelegate> deleted_delegate = std::move(*iter);
  password_change_delegates_.erase(iter);
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, std::move(deleted_delegate));
}
