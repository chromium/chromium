// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/signin/merge_session_navigation_throttle.h"

#include "base/time/time.h"
#include "chrome/browser/ash/login/signin/merge_session_throttling_utils.h"
#include "chrome/browser/ash/login/signin/oauth2_login_manager_factory.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace {

// Maximum wait time for merge session process.
constexpr base::TimeDelta kTotalWaitTime = base::TimeDelta::FromSeconds(10);

chromeos::OAuth2LoginManager* GetOAuth2LoginManager(
    content::WebContents* web_contents) {
  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  if (!browser_context)
    return nullptr;

  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile)
    return nullptr;

  return chromeos::OAuth2LoginManagerFactory::GetInstance()->GetForProfile(
      profile);
}
}  // namespace

// static
std::unique_ptr<content::NavigationThrottle>
MergeSessionNavigationThrottle::Create(content::NavigationHandle* handle) {
  return std::unique_ptr<content::NavigationThrottle>(
      new MergeSessionNavigationThrottle(handle));
}

MergeSessionNavigationThrottle::MergeSessionNavigationThrottle(
    content::NavigationHandle* handle)
    : NavigationThrottle(handle), login_manager_observer_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

MergeSessionNavigationThrottle::~MergeSessionNavigationThrottle() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

content::NavigationThrottle::ThrottleCheckResult
MergeSessionNavigationThrottle::WillStartRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!merge_session_throttling_utils::ShouldDelayUrl(
          navigation_handle()->GetURL()) ||
      !merge_session_throttling_utils::ShouldDelayRequestForWebContents(
          navigation_handle()->GetWebContents())) {
    return content::NavigationThrottle::PROCEED;
  }

  if (BeforeDefer())
    return content::NavigationThrottle::DEFER;

  return content::NavigationThrottle::PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
MergeSessionNavigationThrottle::WillRedirectRequest() {
  return WillStartRequest();
}

const char* MergeSessionNavigationThrottle::GetNameForLogging() {
  return "MergeSessionNavigationThrottle";
}

void MergeSessionNavigationThrottle::OnSessionRestoreStateChanged(
    Profile* user_profile,
    chromeos::OAuth2LoginManager::SessionRestoreState state) {
  chromeos::OAuth2LoginManager* manager =
      GetOAuth2LoginManager(navigation_handle()->GetWebContents());
  if (!manager->ShouldBlockTabLoading()) {
    Proceed();
  }
}

bool MergeSessionNavigationThrottle::BeforeDefer() {
  chromeos::OAuth2LoginManager* manager =
      GetOAuth2LoginManager(navigation_handle()->GetWebContents());
  if (manager && manager->ShouldBlockTabLoading()) {
    login_manager_observer_.Add(manager);
    proceed_timer_.Start(FROM_HERE, kTotalWaitTime, this,
                         &MergeSessionNavigationThrottle::Proceed);
    return true;
  }
  return false;
}

void MergeSessionNavigationThrottle::Proceed() {
  proceed_timer_.Stop();
  chromeos::OAuth2LoginManager* manager =
      GetOAuth2LoginManager(navigation_handle()->GetWebContents());
  if (manager) {
    login_manager_observer_.Remove(manager);
  }
  Resume();
}
