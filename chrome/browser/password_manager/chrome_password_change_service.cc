// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/chrome_password_change_service.h"

#include "base/command_line.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/password_change_delegate.h"
#include "chrome/browser/password_manager/password_change_delegate_impl.h"
#include "chrome/common/chrome_switches.h"
#include "components/affiliations/core/browser/affiliation_service.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
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

bool HasURLFromCommandArgs(const GURL& url) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kPasswordChangeUrl)) {
    return false;
  }
  GURL change_password_url =
      GURL(command_line->GetSwitchValueASCII(switches::kPasswordChangeUrl));

  if (!change_password_url.is_valid()) {
    return false;
  }

  return affiliations::IsExtendedPublicSuffixDomainMatch(
      url, change_password_url, {});
}

GURL GetURLFromCommandArgs(const GURL& url) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  return GURL(command_line->GetSwitchValueASCII(switches::kPasswordChangeUrl));
}

}  // namespace

ChromePasswordChangeService::ChromePasswordChangeService(
    affiliations::AffiliationService* affiliation_service,
    OptimizationGuideKeyedService* optimization_keyed_service)
    : affiliation_service_(affiliation_service),
      optimization_keyed_service_(optimization_keyed_service),
      new_tab_callback_(base::BindRepeating(&OpenNewTab)) {}

ChromePasswordChangeService::~ChromePasswordChangeService() {
  CHECK(password_change_delegates_.empty());
}

bool ChromePasswordChangeService::IsPasswordChangeSupported(const GURL& url) {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kImprovedPasswordChangeService)) {
    return false;
  }

  if (HasURLFromCommandArgs(url)) {
    return true;
  }

  const bool is_user_allowed =
      optimization_keyed_service_ &&
      optimization_keyed_service_
          ->ShouldFeatureAllowModelExecutionForSignedInUser(
              optimization_guide::UserVisibleFeatureKey::
                  kPasswordChangeSubmission);
  const bool is_url_supported =
      affiliation_service_->GetChangePasswordURL(url).is_valid();
  return is_url_supported && is_user_allowed;
}

void ChromePasswordChangeService::OfferPasswordChangeUi(
    const GURL& url,
    const std::u16string& username,
    const std::u16string& password,
    content::WebContents* web_contents) {
  GURL change_pwd_url = HasURLFromCommandArgs(url)
                            ? GetURLFromCommandArgs(url)
                            : affiliation_service_->GetChangePasswordURL(url);
  CHECK(change_pwd_url.is_valid());

  std::unique_ptr<PasswordChangeDelegate> delegate =
      std::make_unique<PasswordChangeDelegateImpl>(
          std::move(change_pwd_url), username, password, web_contents,
          new_tab_callback_);
  delegate->AddObserver(this);
  password_change_delegates_.push_back(std::move(delegate));

  // Init only after `delegate` was added to the vector, so it can be
  // immediately returned by GetPasswordChangeDelegate() when the flow starts.
  static_cast<PasswordChangeDelegateImpl*>(
      password_change_delegates_.back().get())
      ->OfferPasswordChangeUi();
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

  auto iter = std::ranges::find(password_change_delegates_, delegate,
                                &std::unique_ptr<PasswordChangeDelegate>::get);
  CHECK(iter != password_change_delegates_.end());

  std::unique_ptr<PasswordChangeDelegate> deleted_delegate = std::move(*iter);
  password_change_delegates_.erase(iter);
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, std::move(deleted_delegate));
}

void ChromePasswordChangeService::Shutdown() {
  for (const auto& delegate : password_change_delegates_) {
    delegate->RemoveObserver(this);
  }
  password_change_delegates_.clear();
}
