// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_request_manager.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/containers/circular_deque.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string16.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker.h"
#include "chrome/browser/permissions/permission_request.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/permission_bubble/permission_prompt.h"
#include "chrome/common/chrome_switches.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "url/origin.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/extensions/extension_ui_util.h"
#include "extensions/common/constants.h"
#endif

namespace {

bool IsMessageTextEqual(PermissionRequest* a,
                        PermissionRequest* b) {
  if (a == b)
    return true;
  if (a->GetMessageTextFragment() == b->GetMessageTextFragment() &&
      a->GetOrigin() == b->GetOrigin()) {
    return true;
  }
  return false;
}

// We only group together media requests. We don't display grouped requests for
// any other permissions at present.
bool ShouldGroupRequests(PermissionRequest* a, PermissionRequest* b) {
  if (a->GetOrigin() != b->GetOrigin())
    return false;

  if (a->GetPermissionRequestType() ==
      PermissionRequestType::PERMISSION_MEDIASTREAM_MIC) {
    return b->GetPermissionRequestType() ==
           PermissionRequestType::PERMISSION_MEDIASTREAM_CAMERA;
  }

  if (a->GetPermissionRequestType() ==
      PermissionRequestType::PERMISSION_MEDIASTREAM_CAMERA) {
    return b->GetPermissionRequestType() ==
           PermissionRequestType::PERMISSION_MEDIASTREAM_MIC;
  }

  return false;
}

}  // namespace

// PermissionRequestManager ----------------------------------------------------

PermissionRequestManager::PermissionRequestManager(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      view_factory_(base::Bind(&PermissionPrompt::Create)),
      view_(nullptr),
      main_frame_has_fully_loaded_(false),
      tab_is_hidden_(web_contents->GetVisibility() ==
                     content::Visibility::HIDDEN),
      auto_response_for_test_(NONE),
      weak_factory_(this) {}

PermissionRequestManager::~PermissionRequestManager() {
  DCHECK(requests_.empty());
  DCHECK(duplicate_requests_.empty());
  DCHECK(queued_requests_.empty());
}

void PermissionRequestManager::AddRequest(PermissionRequest* request) {

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDenyPermissionPrompts)) {
    request->PermissionDenied();
    request->RequestFinished();
    return;
  }

  // TODO(tsergeant): change the UMA to no longer mention bubbles.
  base::RecordAction(base::UserMetricsAction("PermissionBubbleRequest"));

  // TODO(gbillock): is there a race between an early request on a
  // newly-navigated page and the to-be-cleaned-up requests on the previous
  // page? We should maybe listen to DidStartNavigationToPendingEntry (and
  // any other renderer-side nav initiations?). Double-check this for
  // correct behavior on interstitials -- we probably want to basically queue
  // any request for which GetVisibleURL != GetLastCommittedURL.
  const GURL& request_url_ = web_contents()->GetLastCommittedURL();
  bool is_main_frame =
      url::Origin::Create(request_url_)
          .IsSameOriginWith(url::Origin::Create(request->GetOrigin()));

  // Don't re-add an existing request or one with a duplicate text request.
  PermissionRequest* existing_request = GetExistingRequest(request);
  if (existing_request) {
    // |request| is a duplicate. Add it to |duplicate_requests_| unless it's the
    // same object as |existing_request| or an existing duplicate.
    if (request == existing_request)
      return;
    auto range = duplicate_requests_.equal_range(existing_request);
    for (auto it = range.first; it != range.second; ++it) {
      if (request == it->second)
        return;
    }
    duplicate_requests_.insert(std::make_pair(existing_request, request));
    return;
  }

  if (is_main_frame) {
    if (IsBubbleVisible()) {
      base::RecordAction(
          base::UserMetricsAction("PermissionBubbleRequestQueued"));
    }
  } else {
    base::RecordAction(
        base::UserMetricsAction("PermissionBubbleIFrameRequestQueued"));
  }
  queued_requests_.push_back(request);

  if (!IsBubbleVisible())
    ScheduleShowBubble();
}

void PermissionRequestManager::UpdateAnchorPosition() {
  if (view_)
    view_->UpdateAnchorPosition();
}

bool PermissionRequestManager::IsBubbleVisible() {
  return view_ && !requests_.empty();
}

gfx::NativeWindow PermissionRequestManager::GetBubbleWindow() {
  if (view_)
    return view_->GetNativeWindow();
  return nullptr;
}

void PermissionRequestManager::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  CleanUpRequests();
  main_frame_has_fully_loaded_ = false;
}

void PermissionRequestManager::DocumentOnLoadCompletedInMainFrame() {
  main_frame_has_fully_loaded_ = true;
  // This is scheduled because while all calls to the browser have been
  // issued at DOMContentLoaded, they may be bouncing around in scheduled
  // callbacks finding the UI thread still. This makes sure we allow those
  // scheduled calls to AddRequest to complete before we show the page-load
  // permissions bubble.
  ScheduleShowBubble();
}

void PermissionRequestManager::DocumentLoadedInFrame(
    content::RenderFrameHost* render_frame_host) {
  ScheduleShowBubble();
}

void PermissionRequestManager::WebContentsDestroyed() {
  // If the web contents has been destroyed, treat the bubble as cancelled.
  CleanUpRequests();

  // The WebContents is going away; be aggressively paranoid and delete
  // ourselves lest other parts of the system attempt to add permission bubbles
  // or use us otherwise during the destruction.
  web_contents()->RemoveUserData(UserDataKey());
  // That was the equivalent of "delete this". This object is now destroyed;
  // returning from this function is the only safe thing to do.
}

void PermissionRequestManager::OnVisibilityChanged(
    content::Visibility visibility) {
  bool tab_was_hidden = tab_is_hidden_;
  tab_is_hidden_ = visibility == content::Visibility::HIDDEN;
  if (tab_was_hidden == tab_is_hidden_)
    return;

  if (tab_is_hidden_) {
#if !defined(OS_ANDROID)
    if (view_)
      DeleteBubble();
#endif
    return;
  }

  if (!main_frame_has_fully_loaded_)
    return;

  if (requests_.empty()) {
    DequeueRequestsAndShowBubble();
    return;
  }

#if defined(OS_ANDROID)
  // We switched tabs away and back while a prompt was active.
  DCHECK(view_);
#else
  ShowBubble(/*is_reshow=*/true);
#endif
}

const std::vector<PermissionRequest*>& PermissionRequestManager::Requests() {
  return requests_;
}

PermissionPrompt::DisplayNameOrOrigin
PermissionRequestManager::GetDisplayNameOrOrigin() {
  DCHECK(!requests_.empty());
  GURL origin_url = requests_[0]->GetOrigin();

#if !defined(OS_ANDROID)
  if (origin_url.SchemeIs(extensions::kExtensionScheme)) {
    base::string16 extension_name =
        extensions::ui_util::GetEnabledExtensionNameForUrl(
            origin_url, web_contents()->GetBrowserContext());
    if (!extension_name.empty())
      return {extension_name, false /* is_origin */};
  }
#endif  // !defined(OS_ANDROID)

  // Web URLs should be displayed as the origin in the URL.
  return {url_formatter::FormatUrlForSecurityDisplay(
              origin_url, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC),
          true /* is_origin */};
}

void PermissionRequestManager::Accept() {
  DCHECK(view_);
  std::vector<PermissionRequest*>::iterator requests_iter;
  for (requests_iter = requests_.begin(); requests_iter != requests_.end();
       requests_iter++) {
    PermissionGrantedIncludingDuplicates(*requests_iter);
  }
  FinalizeBubble(PermissionAction::GRANTED);
}

void PermissionRequestManager::Deny() {
  DCHECK(view_);
  std::vector<PermissionRequest*>::iterator requests_iter;
  for (requests_iter = requests_.begin();
       requests_iter != requests_.end();
       requests_iter++) {
    PermissionDeniedIncludingDuplicates(*requests_iter);
  }
  FinalizeBubble(PermissionAction::DENIED);
}

void PermissionRequestManager::Closing() {
#if defined(OS_MACOSX)
  // Mac calls this whenever you press Esc.
  if (!view_)
    return;
#endif

  DCHECK(view_);
  std::vector<PermissionRequest*>::iterator requests_iter;
  for (requests_iter = requests_.begin();
       requests_iter != requests_.end();
       requests_iter++) {
    CancelledIncludingDuplicates(*requests_iter);
  }
  FinalizeBubble(PermissionAction::DISMISSED);
}

void PermissionRequestManager::ScheduleShowBubble() {
  // ::ScheduleShowBubble() will be called again when the main frame will be
  // loaded.
  if (!main_frame_has_fully_loaded_)
    return;

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&PermissionRequestManager::DequeueRequestsAndShowBubble,
                     weak_factory_.GetWeakPtr()));
}

void PermissionRequestManager::DequeueRequestsAndShowBubble() {
  if (view_)
    return;
  if (!main_frame_has_fully_loaded_ || tab_is_hidden_)
    return;
  if (queued_requests_.empty())
    return;

  DCHECK(requests_.empty());
  requests_.push_back(queued_requests_.front());
  queued_requests_.pop_front();

  if (!queued_requests_.empty() &&
      ShouldGroupRequests(requests_.front(), queued_requests_.front())) {
    requests_.push_back(queued_requests_.front());
    queued_requests_.pop_front();
  }

  ShowBubble(/*is_reshow=*/false);
}

void PermissionRequestManager::ShowBubble(bool is_reshow) {
  DCHECK(!view_);
  DCHECK(!requests_.empty());
  DCHECK(main_frame_has_fully_loaded_);
  DCHECK(!tab_is_hidden_);

  view_ = view_factory_.Run(web_contents(), this);
  if (!is_reshow)
    PermissionUmaUtil::PermissionPromptShown(requests_);
  NotifyBubbleAdded();

  // If in testing mode, automatically respond to the bubble that was shown.
  if (auto_response_for_test_ != NONE)
    DoAutoResponseForTesting();
}

void PermissionRequestManager::DeleteBubble() {
  DCHECK(view_);
  view_.reset();
  NotifyBubbleRemoved();
}

void PermissionRequestManager::FinalizeBubble(
    PermissionAction permission_action) {
  DCHECK(!requests_.empty());

  if (view_)
    DeleteBubble();

  PermissionUmaUtil::PermissionPromptResolved(requests_, web_contents(),
                                              permission_action);

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  PermissionDecisionAutoBlocker* autoblocker =
      PermissionDecisionAutoBlocker::GetForProfile(profile);

  for (PermissionRequest* request : requests_) {
    // TODO(timloh): We only support dismiss and ignore embargo for permissions
    // which use PermissionRequestImpl as the other subclasses don't support
    // GetContentSettingsType.
    if (request->GetContentSettingsType() == CONTENT_SETTINGS_TYPE_DEFAULT)
      continue;

    PermissionEmbargoStatus embargo_status =
        PermissionEmbargoStatus::NOT_EMBARGOED;
    if (permission_action == PermissionAction::DISMISSED) {
      if (autoblocker->RecordDismissAndEmbargo(
              request->GetOrigin(), request->GetContentSettingsType())) {
        embargo_status = PermissionEmbargoStatus::REPEATED_DISMISSALS;
      }
    } else if (permission_action == PermissionAction::IGNORED) {
      if (autoblocker->RecordIgnoreAndEmbargo(
              request->GetOrigin(), request->GetContentSettingsType())) {
        embargo_status = PermissionEmbargoStatus::REPEATED_IGNORES;
      }
    }
    PermissionUmaUtil::RecordEmbargoStatus(embargo_status);
  }

  std::vector<PermissionRequest*>::iterator requests_iter;
  for (requests_iter = requests_.begin();
       requests_iter != requests_.end();
       requests_iter++) {
    RequestFinishedIncludingDuplicates(*requests_iter);
  }
  requests_.clear();
  if (queued_requests_.size())
    DequeueRequestsAndShowBubble();
}

void PermissionRequestManager::CleanUpRequests() {
  for (PermissionRequest* request : queued_requests_)
    RequestFinishedIncludingDuplicates(request);
  queued_requests_.clear();

  if (!requests_.empty())
    FinalizeBubble(PermissionAction::IGNORED);
}

PermissionRequest* PermissionRequestManager::GetExistingRequest(
    PermissionRequest* request) {
  for (PermissionRequest* existing_request : requests_) {
    if (IsMessageTextEqual(existing_request, request))
      return existing_request;
  }
  for (PermissionRequest* existing_request : queued_requests_) {
    if (IsMessageTextEqual(existing_request, request))
      return existing_request;
  }
  return nullptr;
}

void PermissionRequestManager::PermissionGrantedIncludingDuplicates(
    PermissionRequest* request) {
  DCHECK_EQ(request, GetExistingRequest(request))
      << "Only requests in [queued_[frame_]]requests_ can have duplicates";
  request->PermissionGranted();
  auto range = duplicate_requests_.equal_range(request);
  for (auto it = range.first; it != range.second; ++it)
    it->second->PermissionGranted();
}

void PermissionRequestManager::PermissionDeniedIncludingDuplicates(
    PermissionRequest* request) {
  DCHECK_EQ(request, GetExistingRequest(request))
      << "Only requests in [queued_]requests_ can have duplicates";
  request->PermissionDenied();
  auto range = duplicate_requests_.equal_range(request);
  for (auto it = range.first; it != range.second; ++it)
    it->second->PermissionDenied();
}

void PermissionRequestManager::CancelledIncludingDuplicates(
    PermissionRequest* request) {
  DCHECK_EQ(request, GetExistingRequest(request))
      << "Only requests in [queued_]requests_ can have duplicates";
  request->Cancelled();
  auto range = duplicate_requests_.equal_range(request);
  for (auto it = range.first; it != range.second; ++it)
    it->second->Cancelled();
}

void PermissionRequestManager::RequestFinishedIncludingDuplicates(
    PermissionRequest* request) {
  // We can't call GetExistingRequest here, because other entries in requests_,
  // queued_requests_ might already have been deleted.
  DCHECK_EQ(1, std::count(requests_.begin(), requests_.end(), request) +
                   std::count(queued_requests_.begin(), queued_requests_.end(),
                              request))
      << "Only requests in [queued_]requests_ can have duplicates";
  request->RequestFinished();
  // Beyond this point, |request| has probably been deleted.
  auto range = duplicate_requests_.equal_range(request);
  for (auto it = range.first; it != range.second; ++it)
    it->second->RequestFinished();
  // Additionally, we can now remove the duplicates.
  duplicate_requests_.erase(request);
}

void PermissionRequestManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void PermissionRequestManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void PermissionRequestManager::NotifyBubbleAdded() {
  for (Observer& observer : observer_list_)
    observer.OnBubbleAdded();
}

void PermissionRequestManager::NotifyBubbleRemoved() {
  for (Observer& observer : observer_list_)
    observer.OnBubbleRemoved();
}

void PermissionRequestManager::DoAutoResponseForTesting() {
  switch (auto_response_for_test_) {
    case ACCEPT_ALL:
      Accept();
      break;
    case DENY_ALL:
      Deny();
      break;
    case DISMISS:
      Closing();
      break;
    case NONE:
      NOTREACHED();
  }
}
