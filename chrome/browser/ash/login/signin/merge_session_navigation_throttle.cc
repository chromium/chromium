// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/signin/merge_session_navigation_throttle.h"

#include "base/time/time.h"
#include "chrome/browser/ash/login/signin/merge_session_throttling_utils.h"
#include "chrome/browser/ash/login/signin/oauth2_login_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace ash {
namespace {

// Maximum wait time for merge session process.
constexpr base::TimeDelta kTotalWaitTime = base::Seconds(10);

OAuth2LoginManager* GetOAuth2LoginManager(content::WebContents* web_contents) {
  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  if (!browser_context)
    return nullptr;

  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile)
    return nullptr;

  return OAuth2LoginManagerFactory::GetInstance()->GetForProfile(profile);
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
    : NavigationThrottle(handle) {
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
    OAuth2LoginManager::SessionRestoreState state) {
  auto* manager = GetOAuth2LoginManager(navigation_handle()->GetWebContents());
  if (!manager->ShouldBlockTabLoading()) {
    Proceed();
  }
}

bool MergeSessionNavigationThrottle::BeforeDefer() {
  auto* manager = GetOAuth2LoginManager(navigation_handle()->GetWebContents());
  if (manager && manager->ShouldBlockTabLoading()) {
    login_manager_observation_.Observe(manager);
    proceed_timer_.Start(FROM_HERE, kTotalWaitTime, this,
                         &MergeSessionNavigationThrottle::Proceed);
    return true;
  }
  return false;
}

void MergeSessionNavigationThrottle::Proceed() {
  proceed_timer_.Stop();
  auto* manager = GetOAuth2LoginManager(navigation_handle()->GetWebContents());
  if (manager) {
    DCHECK(login_manager_observation_.IsObservingSource(manager));
    login_manager_observation_.Reset();
  }
  Resume();
}

}  // namespace ash
