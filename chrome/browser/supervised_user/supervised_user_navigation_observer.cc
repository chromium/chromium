// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_interstitial.h"
#include "chrome/browser/supervised_user/supervised_user_navigation_throttle.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/chrome_features.h"
#include "components/history/content/browser/history_context_helper.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"

using content::NavigationEntry;

SupervisedUserNavigationObserver::~SupervisedUserNavigationObserver() {
  supervised_user_service_->RemoveObserver(this);
}

SupervisedUserNavigationObserver::SupervisedUserNavigationObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      binding_(web_contents, this),
      weak_ptr_factory_(this) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  supervised_user_service_ =
      SupervisedUserServiceFactory::GetForProfile(profile);
  url_filter_ = supervised_user_service_->GetURLFilter();
  supervised_user_service_->AddObserver(this);
}

// static
void SupervisedUserNavigationObserver::OnRequestBlocked(
    content::WebContents* web_contents,
    const GURL& url,
    supervised_user_error_page::FilteringBehaviorReason reason,
    int64_t navigation_id,
    const base::Callback<
        void(SupervisedUserNavigationThrottle::CallbackActions)>& callback) {
  SupervisedUserNavigationObserver* navigation_observer =
      SupervisedUserNavigationObserver::FromWebContents(web_contents);

  // Cancel the navigation if there is no navigation observer.
  if (!navigation_observer) {
    callback.Run(
        SupervisedUserNavigationThrottle::CallbackActions::kCancelNavigation);
    return;
  }

  navigation_observer->OnRequestBlockedInternal(url, reason, navigation_id,
                                                callback);
}

void SupervisedUserNavigationObserver::DidFinishNavigation(
      content::NavigationHandle* navigation_handle) {
  // With committed interstitials on, if this is a different navigation than the
  // one that triggered the interstitial, clear is_showing_interstitial_
  if (is_showing_interstitial_ &&
      navigation_handle->GetNavigationId() != interstitial_navigation_id_ &&
      base::FeatureList::IsEnabled(
          features::kSupervisedUserCommittedInterstitials)) {
    is_showing_interstitial_ = false;
  }

  // Only filter same page navigations (eg. pushState/popState); others will
  // have been filtered by the NavigationThrottle.
  if (navigation_handle->IsSameDocument() &&
      navigation_handle->IsInMainFrame()) {
    url_filter_->GetFilteringBehaviorForURLWithAsyncChecks(
        web_contents()->GetLastCommittedURL(),
        base::BindOnce(
            &SupervisedUserNavigationObserver::URLFilterCheckCallback,
            weak_ptr_factory_.GetWeakPtr(), navigation_handle->GetURL()));
  }
}

void SupervisedUserNavigationObserver::OnURLFilterChanged() {
  url_filter_->GetFilteringBehaviorForURLWithAsyncChecks(
      web_contents()->GetLastCommittedURL(),
      base::BindOnce(&SupervisedUserNavigationObserver::URLFilterCheckCallback,
                     weak_ptr_factory_.GetWeakPtr(),
                     web_contents()->GetLastCommittedURL()));
}

void SupervisedUserNavigationObserver::OnRequestBlockedInternal(
    const GURL& url,
    supervised_user_error_page::FilteringBehaviorReason reason,
    int64_t navigation_id,
    const base::Callback<
        void(SupervisedUserNavigationThrottle::CallbackActions)>& callback) {
  // TODO(bauerb): Use SaneTime when available.
  base::Time timestamp = base::Time::Now();
  // Create a history entry for the attempt and mark it as such.  This history
  // entry should be marked as "not hidden" so the user can see attempted but
  // blocked navigations.  (This is in contrast to the normal behavior, wherein
  // Chrome marks navigations that result in an error as hidden.)  This is to
  // show the user the same thing that the custodian will see on the dashboard
  // (where it gets via a different mechanism unrelated to history).
  history::HistoryAddPageArgs add_page_args(
      url, timestamp, history::ContextIDForWebContents(web_contents()), 0, url,
      history::RedirectList(), ui::PAGE_TRANSITION_BLOCKED, false,
      history::SOURCE_BROWSED, false, true);

  // Add the entry to the history database.
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::IMPLICIT_ACCESS);

  // |history_service| is null if saving history is disabled.
  if (history_service)
    history_service->AddPage(add_page_args);

  std::unique_ptr<NavigationEntry> entry = NavigationEntry::Create();
  entry->SetVirtualURL(url);
  entry->SetTimestamp(timestamp);
  auto serialized_entry = std::make_unique<sessions::SerializedNavigationEntry>(
      sessions::ContentSerializedNavigationBuilder::FromNavigationEntry(
          blocked_navigations_.size(), *entry));
  blocked_navigations_.push_back(std::move(serialized_entry));

  // Show the interstitial.
  const bool initial_page_load = true;
  MaybeShowInterstitial(url, reason, initial_page_load, navigation_id,
                        callback);
}

void SupervisedUserNavigationObserver::URLFilterCheckCallback(
    const GURL& url,
    SupervisedUserURLFilter::FilteringBehavior behavior,
    supervised_user_error_page::FilteringBehaviorReason reason,
    bool uncertain) {
  // If the page has been changed in the meantime, we can exit.
  if (url != web_contents()->GetLastCommittedURL())
    return;

  if (!is_showing_interstitial_ &&
      behavior == SupervisedUserURLFilter::FilteringBehavior::BLOCK) {
    if (base::FeatureList::IsEnabled(
            features::kSupervisedUserCommittedInterstitials)) {
      web_contents()->GetController().Reload(content::ReloadType::NORMAL,
                                             false);
      return;
    }
    // TODO(carlosil): For now, we pass a 0 as the navigation id causing the
    // interstitial for the non-committed interstitials case since we don't have
    // the real id here, this doesn't cause issues since the navigation id is
    // not used when committed interstitials are not enabled. This will be
    // removed once committed interstitials are the only code path.
    const bool initial_page_load = false;
    MaybeShowInterstitial(
        url, reason, initial_page_load, 0,
        base::Callback<void(
            SupervisedUserNavigationThrottle::CallbackActions)>());
  }
}

void SupervisedUserNavigationObserver::MaybeShowInterstitial(
    const GURL& url,
    supervised_user_error_page::FilteringBehaviorReason reason,
    bool initial_page_load,
    int64_t navigation_id,
    const base::Callback<
        void(SupervisedUserNavigationThrottle::CallbackActions)>& callback) {
  interstitial_navigation_id_ = navigation_id;
  is_showing_interstitial_ = true;
  base::Callback<void(bool)> wrapped_callback =
      base::Bind(&SupervisedUserNavigationObserver::OnInterstitialResult,
                 weak_ptr_factory_.GetWeakPtr(), callback);
  if (base::FeatureList::IsEnabled(
          features::kSupervisedUserCommittedInterstitials)) {
    interstitial_ = SupervisedUserInterstitial::Create(
        web_contents(), url, reason, initial_page_load, wrapped_callback);
    callback.Run(SupervisedUserNavigationThrottle::CallbackActions::
                     kCancelWithInterstitial);
    return;
  }
  SupervisedUserInterstitial::Show(web_contents(), url, reason,
                                   initial_page_load, wrapped_callback);
}

void SupervisedUserNavigationObserver::OnInterstitialResult(
    const base::Callback<
        void(SupervisedUserNavigationThrottle::CallbackActions)>& callback,
    bool result) {
  is_showing_interstitial_ = false;
  // If committed interstitials are enabled, there is no navigation to cancel or
  // defer at this point, so just clear the is_showing_interstitial variable.
  if (callback && !base::FeatureList::IsEnabled(
                      features::kSupervisedUserCommittedInterstitials))
    callback.Run(result ? SupervisedUserNavigationThrottle::CallbackActions::
                              kContinueNavigation
                        : SupervisedUserNavigationThrottle::CallbackActions::
                              kCancelNavigation);
}

void SupervisedUserNavigationObserver::GoBack() {
  DCHECK(base::FeatureList::IsEnabled(
      features::kSupervisedUserCommittedInterstitials));
  if (interstitial_ && is_showing_interstitial_)
    interstitial_->CommandReceived("\"back\"");
}

void SupervisedUserNavigationObserver::RequestPermission(
    RequestPermissionCallback callback) {
  DCHECK(base::FeatureList::IsEnabled(
      features::kSupervisedUserCommittedInterstitials));
  if (interstitial_ && is_showing_interstitial_)
    interstitial_->RequestPermission(std::move(callback));
}

void SupervisedUserNavigationObserver::Feedback() {
  DCHECK(base::FeatureList::IsEnabled(
      features::kSupervisedUserCommittedInterstitials));
  if (interstitial_ && is_showing_interstitial_)
    interstitial_->CommandReceived("\"feedback\"");
}
