// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_request_limiter.h"

#include <iterator>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/chrome_content_settings_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/download/download_permission_request.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "url/gurl.h"

using content::BrowserThread;
using content::NavigationController;
using content::NavigationEntry;

namespace {

ContentSetting GetSettingFromDownloadStatus(
    DownloadRequestLimiter::DownloadStatus status) {
  switch (status) {
    case DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD:
    case DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD:
      return CONTENT_SETTING_ASK;
    case DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS:
      return CONTENT_SETTING_ALLOW;
    case DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED:
      return CONTENT_SETTING_BLOCK;
  }
  NOTREACHED_IN_MIGRATION();
  return CONTENT_SETTING_DEFAULT;
}

DownloadRequestLimiter::DownloadStatus GetDownloadStatusFromSetting(
    ContentSetting setting) {
  switch (setting) {
    case CONTENT_SETTING_ALLOW:
      return DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS;
    case CONTENT_SETTING_BLOCK:
      return DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED;
    case CONTENT_SETTING_DEFAULT:
    case CONTENT_SETTING_ASK:
      return DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD;
    case CONTENT_SETTING_SESSION_ONLY:
    case CONTENT_SETTING_NUM_SETTINGS:
    case CONTENT_SETTING_DETECT_IMPORTANT_CONTENT:
      NOTREACHED_IN_MIGRATION();
      return DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD;
  }
  NOTREACHED_IN_MIGRATION();
  return DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD;
}

DownloadRequestLimiter::DownloadUiStatus GetUiStatusFromDownloadStatus(
    DownloadRequestLimiter::DownloadStatus status,
    bool download_seen) {
  if (!download_seen)
    return DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT;

  switch (status) {
    case DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS:
      return DownloadRequestLimiter::DOWNLOAD_UI_ALLOWED;
    case DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED:
      return DownloadRequestLimiter::DOWNLOAD_UI_BLOCKED;
    case DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD:
    case DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD:
      return DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT;
  }
  NOTREACHED_IN_MIGRATION();
  return DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT;
}

}  // namespace

// TabDownloadState ------------------------------------------------------------

DownloadRequestLimiter::TabDownloadState::TabDownloadState(
    DownloadRequestLimiter* host,
    content::WebContents* contents)
    : content::WebContentsObserver(contents),
      web_contents_(contents),
      host_(host),
      status_(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD),
      ui_status_(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT),
      origin_(url::Origin::Create(contents->GetVisibleURL())),
      download_count_(0),
      download_seen_(false) {
  observation_.Observe(GetContentSettings(contents));
}

DownloadRequestLimiter::TabDownloadState::~TabDownloadState() {
  // We should only be destroyed after the callbacks have been notified.
  DCHECK(callbacks_.empty());

  // And we should have invalidated the back pointer.
  DCHECK(!factory_.HasWeakPtrs());
}

void DownloadRequestLimiter::TabDownloadState::SetDownloadStatusAndNotify(
    const url::Origin& request_origin,
    DownloadStatus status) {
  SetDownloadStatusAndNotifyImpl(request_origin, status,
                                 GetSettingFromDownloadStatus(status));
}

void DownloadRequestLimiter::TabDownloadState::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame())
    return;

  download_seen_ = false;
  ui_status_ = DOWNLOAD_UI_DEFAULT;

  if (navigation_handle->IsRendererInitiated()) {
    return;
  }

  // If this is a forward/back navigation, also don't reset a prompting or
  // blocking limiter state if an origin is limited. This prevents a page
  // to use history forward/backward to trigger multiple downloads.
  if (!shouldClearDownloadState(navigation_handle))
    return;

  NotifyCallbacks(false);
  host_->Remove(this, web_contents());
}

void DownloadRequestLimiter::TabDownloadState::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame())
    return;

  // If this is a forward/back navigation, also don't reset a prompting or
  // blocking limiter state if an origin is limited. This prevents a page
  // to use history forward/backward to trigger multiple downloads.
  if (!shouldClearDownloadState(navigation_handle))
    return;

  // Treat browser-initiated navigations as user interactions as long as the
  // navigation can clear download state.
  if (!navigation_handle->IsRendererInitiated()) {
    OnUserInteraction();
    return;
  }

  // When the status is ALLOW_ALL_DOWNLOADS or DOWNLOADS_NOT_ALLOWED, don't drop
  // this information. The user has explicitly said that they do/don't want
  // downloads from this host. If they accidentally Accepted or Canceled, they
  // can adjust the limiter state by adjusting the automatic downloads content
  // settings. Alternatively, they can copy the URL into a new tab, which will
  // make a new DownloadRequestLimiter.
  if (status_ == ALLOW_ONE_DOWNLOAD) {
    // When the user reloads the page without responding to the prompt,
    // they are expecting DownloadRequestLimiter to behave as if they had
    // just initially navigated to this page. See http://crbug.com/171372.
    // However, explicitly leave the limiter in place if the navigation was
    // renderer-initiated and we are in a prompt state.
    NotifyCallbacks(false);
    host_->Remove(this, web_contents());
    return;
    // WARNING: We've been deleted.
  } else if (status_ == ALLOW_ALL_DOWNLOADS) {
    OnUserInteraction();
    return;
  }
}

void DownloadRequestLimiter::TabDownloadState::DidGetUserInteraction(
    const blink::WebInputEvent& event) {
  if (is_showing_prompt() ||
      event.GetType() == blink::WebInputEvent::Type::kGestureScrollBegin) {
    // Don't change state if a prompt is showing or if the user has scrolled.
    return;
  }

  OnUserInteraction();
}

void DownloadRequestLimiter::TabDownloadState::WebContentsDestroyed() {
  // Tab closed, no need to handle closing the dialog as it's owned by the
  // WebContents.

  NotifyCallbacks(false);
  host_->Remove(this, web_contents());
  // WARNING: We've been deleted.
}

void DownloadRequestLimiter::TabDownloadState::PromptUserForDownload(
    DownloadRequestLimiter::Callback callback,
    const url::Origin& request_origin) {
  callbacks_.push_back(std::move(callback));
  DCHECK(web_contents_);
  if (is_showing_prompt())
    return;

  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents_);
  if (permission_request_manager) {
    // The RFH is used to scope the lifetime of the request and scoping it to
    // the initiator doesn't make sense for downloads as download navigation
    // requests are never committed and don't update the omnibox url.
    // Download requests should only be granted by checking `request_origin`,
    // so we use the primary main RenderFrameHost here, to avoid discarding the
    // request in the case that the initiator RFH is already gone.
    permission_request_manager->AddRequest(
        web_contents_->GetPrimaryMainFrame(),
        new DownloadPermissionRequest(factory_.GetWeakPtr(), request_origin));
  } else {
    // Call CancelOnce() so we don't set the content settings.
    CancelOnce(request_origin);
  }
}

void DownloadRequestLimiter::TabDownloadState::SetContentSetting(
    ContentSetting setting,
    const url::Origin& request_origin) {
  if (!web_contents_)
    return;
  if (request_origin.opaque())
    return;
  HostContentSettingsMap* settings =
      DownloadRequestLimiter::GetContentSettings(web_contents_);
  if (!settings)
    return;
  settings->SetContentSettingDefaultScope(
      request_origin.GetURL(), GURL(), ContentSettingsType::AUTOMATIC_DOWNLOADS,
      setting);
}

void DownloadRequestLimiter::TabDownloadState::Cancel(
    const url::Origin& request_origin) {
  SetContentSetting(CONTENT_SETTING_BLOCK, request_origin);
  bool throttled = NotifyCallbacks(false);
  SetDownloadStatusAndNotify(request_origin, throttled ? PROMPT_BEFORE_DOWNLOAD
                                                       : DOWNLOADS_NOT_ALLOWED);
}

void DownloadRequestLimiter::TabDownloadState::CancelOnce(
    const url::Origin& request_origin) {
  bool throttled = NotifyCallbacks(false);
  SetDownloadStatusAndNotify(request_origin, throttled ? PROMPT_BEFORE_DOWNLOAD
                                                       : DOWNLOADS_NOT_ALLOWED);
}

void DownloadRequestLimiter::TabDownloadState::Accept(
    const url::Origin& request_origin) {
  SetContentSetting(CONTENT_SETTING_ALLOW, request_origin);
  bool throttled = NotifyCallbacks(true);
  SetDownloadStatusAndNotify(
      request_origin, throttled ? PROMPT_BEFORE_DOWNLOAD : ALLOW_ALL_DOWNLOADS);
}

DownloadRequestLimiter::DownloadStatus
DownloadRequestLimiter::TabDownloadState::GetDownloadStatus(
    const url::Origin& request_origin) {
  auto it = download_status_map_.find(request_origin);
  if (it != download_status_map_.end())
    return it->second;
  return ALLOW_ONE_DOWNLOAD;
}

DownloadRequestLimiter::TabDownloadState::TabDownloadState()
    : web_contents_(nullptr),
      host_(nullptr),
      status_(DownloadRequestLimiter::ALLOW_ONE_DOWNLOAD),
      ui_status_(DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT),
      download_count_(0),
      download_seen_(false) {}

bool DownloadRequestLimiter::TabDownloadState::is_showing_prompt() const {
  return factory_.HasWeakPtrs();
}

void DownloadRequestLimiter::TabDownloadState::OnUserInteraction() {
  // See PromptUserForDownload(): if there's no PermissionRequestManager, then
  // DOWNLOADS_NOT_ALLOWED is functionally equivalent to PROMPT_BEFORE_DOWNLOAD.
  bool no_permission_request_manager =
      (permissions::PermissionRequestManager::FromWebContents(web_contents()) ==
       nullptr);

  for (auto it = download_status_map_.begin();
       it != download_status_map_.end();) {
    ContentSetting setting =
        GetAutoDownloadContentSetting(web_contents(), it->first.GetURL());
    // If an origin has non-block content setting and does not have
    // |DOWNLOADS_NOT_ALLOWED| or |ALLOW_ALL_DOWNLOADS| status, remove
    // it from the map so that it is able to initiate one download
    // without asking the user.
    if (setting != CONTENT_SETTING_BLOCK && it->second != ALLOW_ALL_DOWNLOADS &&
        ((no_permission_request_manager &&
          it->second == DOWNLOADS_NOT_ALLOWED) ||
         it->second != DOWNLOADS_NOT_ALLOWED)) {
      it = download_status_map_.erase(it);
    } else {
      ++it;
    }
  }

  // Reset the download count to 0 so that one download can go through.
  download_count_ = 0;

  if (download_status_map_.empty()) {
    host_->Remove(this, web_contents());
    // WARNING: We've been deleted.
  }
}

void DownloadRequestLimiter::TabDownloadState::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  if (!content_type_set.Contains(ContentSettingsType::AUTOMATIC_DOWNLOADS))
    return;

  if (origin_.opaque())
    return;

  GURL origin = origin_.GetURL();

  // Check if the settings change affects the most recent origin passed
  // to SetDownloadStatusAndNotify(). If so, we need to update the omnibox
  // decoration.
  if (!primary_pattern.Matches(origin))
    return;

  // Content settings have been updated for our web contents, e.g. via the OIB
  // or the settings page. Check to see if the automatic downloads setting is
  // different to our internal state, and update the internal state to match if
  // necessary. If there is no content setting persisted, then retain the
  // current state and do nothing.
  //
  // NotifyCallbacks is not called as this notification should be triggered when
  // a download is not pending.
  //
  // Fetch the content settings map for this web contents, and extract the
  // automatic downloads permission value.
  HostContentSettingsMap* content_settings = GetContentSettings(web_contents());
  if (!content_settings)
    return;

  ContentSetting setting = content_settings->GetContentSetting(
      origin, origin, ContentSettingsType::AUTOMATIC_DOWNLOADS);

  // Update the internal state to match if necessary.
  SetDownloadStatusAndNotifyImpl(origin_, GetDownloadStatusFromSetting(setting),
                                 setting);
}

bool DownloadRequestLimiter::TabDownloadState::NotifyCallbacks(bool allow) {
  std::vector<DownloadRequestLimiter::Callback> callbacks;
  bool throttled = false;

  // Selectively send first few notifications only if number of downloads exceed
  // kMaxDownloadsAtOnce. In that case, we also retain the infobar instance and
  // don't close it. If allow is false, we send all the notifications to cancel
  // all remaining downloads and close the infobar.
  if (!allow || (callbacks_.size() < kMaxDownloadsAtOnce)) {
    // Null the generated weak pointer so we don't get notified again.
    factory_.InvalidateWeakPtrs();
    callbacks.swap(callbacks_);
  } else {
    std::vector<DownloadRequestLimiter::Callback>::iterator start, end;
    start = callbacks_.begin();
    end = callbacks_.begin() + kMaxDownloadsAtOnce;
    callbacks.assign(std::make_move_iterator(start),
                     std::make_move_iterator(end));
    callbacks_.erase(start, end);
    throttled = true;
  }

  for (auto& callback : callbacks) {
    // When callback runs, it can cause the WebContents to be destroyed.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), allow));
  }

  return throttled;
}

void DownloadRequestLimiter::TabDownloadState::SetDownloadStatusAndNotifyImpl(
    const url::Origin& request_origin,
    DownloadStatus status,
    ContentSetting setting) {
  DCHECK((GetSettingFromDownloadStatus(status) == setting) ||
         (GetDownloadStatusFromSetting(setting) == status))
      << "status " << status << " and setting " << setting
      << " do not correspond to each other";
  ContentSetting last_setting = GetSettingFromDownloadStatus(status_);
  DownloadUiStatus last_ui_status = ui_status_;
  url::Origin last_origin = origin_;

  status_ = status;
  ui_status_ = GetUiStatusFromDownloadStatus(status_, download_seen_);
  origin_ = request_origin;

  if (status_ != ALLOW_ONE_DOWNLOAD)
    download_status_map_[request_origin] = status_;
  else
    download_status_map_.erase(request_origin);

  if (!web_contents())
    return;

  // For opaque origins, the omnibox decoration cannot show the URL. As a
  // result, don't send a notification.
  if (origin_.opaque())
    return;

  // We want to send a notification if the UI status has changed to ensure that
  // the omnibox decoration updates appropriately. This is effectively the same
  // as other permissions which might be in an allow state, but do not show UI
  // until they are actively used.
  if (last_setting == setting && last_ui_status == ui_status_ &&
      origin_ == last_origin) {
    return;
  }

  content_settings::UpdateLocationBarUiForWebContents(web_contents());
}

bool DownloadRequestLimiter::TabDownloadState::shouldClearDownloadState(
    content::NavigationHandle* navigation_handle) {
  // For forward/backward navigations, don't clear download state if some
  // origins are restricted.
  if (navigation_handle->GetPageTransition() &
      ui::PAGE_TRANSITION_FORWARD_BACK) {
    for (const auto& entry : download_status_map_) {
      if (entry.second == PROMPT_BEFORE_DOWNLOAD ||
          entry.second == DOWNLOADS_NOT_ALLOWED)
        return false;
    }
  }
  return true;
}

// DownloadRequestLimiter ------------------------------------------------------

DownloadRequestLimiter::DownloadRequestLimiter() {}

DownloadRequestLimiter::~DownloadRequestLimiter() {
  // All the tabs should have closed before us, which sends notification and
  // removes from state_map_. As such, there should be no pending callbacks.
  DCHECK(state_map_.empty());
}

DownloadRequestLimiter::DownloadStatus
DownloadRequestLimiter::GetDownloadStatus(content::WebContents* web_contents) {
  TabDownloadState* state = GetDownloadState(web_contents, false);
  return state ? state->download_status() : ALLOW_ONE_DOWNLOAD;
}

DownloadRequestLimiter::DownloadUiStatus
DownloadRequestLimiter::GetDownloadUiStatus(
    content::WebContents* web_contents) {
  TabDownloadState* state = GetDownloadState(web_contents, false);
  return state ? state->download_ui_status() : DOWNLOAD_UI_DEFAULT;
}

GURL DownloadRequestLimiter::GetDownloadOrigin(
    content::WebContents* web_contents) {
  TabDownloadState* state = GetDownloadState(web_contents, false);
  if (state && !state->origin().opaque())
    return state->origin().GetURL();
  return web_contents->GetVisibleURL();
}

DownloadRequestLimiter::TabDownloadState*
DownloadRequestLimiter::GetDownloadState(
    content::WebContents* web_contents,
    bool create) {
  DCHECK(web_contents);
  auto i = state_map_.find(web_contents);
  if (i != state_map_.end())
    return i->second;

  if (!create)
    return nullptr;

  TabDownloadState* state = new TabDownloadState(this, web_contents);
  state_map_[web_contents] = state;
  return state;
}

void DownloadRequestLimiter::CanDownload(
    const content::WebContents::Getter& web_contents_getter,
    const GURL& url,
    const std::string& request_method,
    std::optional<url::Origin> request_initiator,
    bool from_download_cross_origin_redirect,
    Callback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::WebContents* originating_contents = web_contents_getter.Run();
  if (!originating_contents) {
    // The WebContents was closed, don't allow the download.
    std::move(callback).Run(false);
    return;
  }

  if (!originating_contents->GetDelegate()) {
    std::move(callback).Run(false);
    return;
  }

  // Note that because |originating_contents| might go away before
  // OnCanDownloadDecided is invoked, we look it up by |render_process_host_id|
  // and |render_view_id|.
  base::OnceCallback<void(bool)> can_download_callback = base::BindOnce(
      &DownloadRequestLimiter::OnCanDownloadDecided, factory_.GetWeakPtr(), url,
      web_contents_getter, request_method, std::move(request_initiator),
      from_download_cross_origin_redirect, std::move(callback));

  originating_contents->GetDelegate()->CanDownload(
      url, request_method, std::move(can_download_callback));
}

void DownloadRequestLimiter::OnCanDownloadDecided(
    const GURL& url,
    const content::WebContents::Getter& web_contents_getter,
    const std::string& request_method,
    std::optional<url::Origin> request_initiator,
    bool from_download_cross_origin_redirect,
    Callback orig_callback,
    bool allow) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::WebContents* originating_contents = web_contents_getter.Run();
  if (!originating_contents || !allow) {
    std::move(orig_callback).Run(false);
    return;
  }

  CanDownloadImpl(
      url, originating_contents, request_method, std::move(request_initiator),
      from_download_cross_origin_redirect, std::move(orig_callback));
}

HostContentSettingsMap* DownloadRequestLimiter::GetContentSettings(
    content::WebContents* contents) {
  return HostContentSettingsMapFactory::GetForProfile(
      Profile::FromBrowserContext(contents->GetBrowserContext()));
}

ContentSetting DownloadRequestLimiter::GetAutoDownloadContentSetting(
    content::WebContents* contents,
    const GURL& request_initiator) {
  HostContentSettingsMap* content_settings = GetContentSettings(contents);
  ContentSetting setting = CONTENT_SETTING_ASK;
  if (content_settings) {
    setting = content_settings->GetContentSetting(
        request_initiator, request_initiator,
        ContentSettingsType::AUTOMATIC_DOWNLOADS);
  }
  return setting;
}

void DownloadRequestLimiter::CanDownloadImpl(
    const GURL& url,
    content::WebContents* originating_contents,
    const std::string& request_method,
    std::optional<url::Origin> request_initiator,
    bool from_download_cross_origin_redirect,
    Callback callback) {
  DCHECK(originating_contents);

  // Always allow download resulted from a cross-origin redirect from a previous
  // download attempt, and there's no need to update any state.
  if (from_download_cross_origin_redirect) {
    std::move(callback).Run(true);
    if (!on_can_download_decided_callback_.is_null())
      on_can_download_decided_callback_.Run(true);
    return;
  }

  TabDownloadState* state = GetDownloadState(originating_contents, true);
  state->set_download_seen();
  bool ret = true;

  // If `request_initiator` is empty, this is a browser initiated request.
  // Get the origin from `url` as visible URL of the current tab may not
  // represent the correct WebContents that triggers the download.
  url::Origin origin = request_initiator
                           ? request_initiator.value()
                           : url::Origin::Resolve(url, url::Origin());

  DownloadStatus status = state->GetDownloadStatus(origin);
  bool is_opaque_initiator = origin.opaque();

  // Always check for the content setting first. Having an content setting
  // observer won't work as |request_initiator| might be different from the tab
  // URL.
  ContentSetting setting = is_opaque_initiator
                               ? CONTENT_SETTING_BLOCK
                               : GetAutoDownloadContentSetting(
                                     originating_contents, origin.GetURL());
  // Override the status if content setting is block or allow. If the content
  // setting is always allow, only reset the status if it is
  // DOWNLOADS_NOT_ALLOWED so unnecessary notifications will not be triggered.
  // If the content setting is block, allow only one download to proceed if the
  // current status is ALLOW_ALL_DOWNLOADS.
  if (setting == CONTENT_SETTING_BLOCK && status == ALLOW_ALL_DOWNLOADS) {
    status = ALLOW_ONE_DOWNLOAD;
  } else if (setting == CONTENT_SETTING_ALLOW &&
             status == DOWNLOADS_NOT_ALLOWED) {
    status = ALLOW_ALL_DOWNLOADS;
  }

  // Always call SetDownloadStatusAndNotify since we may need to change the
  // omnibox UI even if the internal state stays the same. For instance, we want
  // to hide the indicator until a download is triggered, even if we know
  // downloads are blocked. This mirrors the behaviour of other omnibox
  // decorations like geolocation.
  switch (status) {
    case ALLOW_ALL_DOWNLOADS:
      if (state->download_count() &&
          !(state->download_count() %
            DownloadRequestLimiter::kMaxDownloadsAtOnce)) {
        state->SetDownloadStatusAndNotify(origin, PROMPT_BEFORE_DOWNLOAD);
      } else {
        state->SetDownloadStatusAndNotify(origin, ALLOW_ALL_DOWNLOADS);
      }
      std::move(callback).Run(true);
      state->increment_download_count();
      break;

    case ALLOW_ONE_DOWNLOAD:
      state->SetDownloadStatusAndNotify(origin, PROMPT_BEFORE_DOWNLOAD);
      // If one download is seen for this WebContent, ALLOW_ONE_DOWNLOAD is the
      // same as PROMPT_BEFORE_DOWNLOAD unless all downloads are allowed for the
      // origin. This is to avoid a page using different origins to initiate
      // multiple downloads.
      if (state->download_count() > 0 && setting != CONTENT_SETTING_ALLOW) {
        ret = false;
        // If setting is CONTENT_SETTING_BLOCK, don't prompt user.
        if (setting == CONTENT_SETTING_BLOCK) {
          state->SetDownloadStatusAndNotify(origin, DOWNLOADS_NOT_ALLOWED);
          std::move(callback).Run(false);
        } else {
          state->PromptUserForDownload(std::move(callback), origin);
          state->increment_download_count();
        }
      } else {
        std::move(callback).Run(true);
        state->increment_download_count();
      }
      break;

    case DOWNLOADS_NOT_ALLOWED:
      state->SetDownloadStatusAndNotify(origin, DOWNLOADS_NOT_ALLOWED);
      ret = false;
      std::move(callback).Run(false);
      break;

    case PROMPT_BEFORE_DOWNLOAD: {
      switch (setting) {
        case CONTENT_SETTING_ALLOW: {
          state->SetDownloadStatusAndNotify(origin, ALLOW_ALL_DOWNLOADS);
          std::move(callback).Run(true);
          state->increment_download_count();
          break;
        }
        case CONTENT_SETTING_BLOCK: {
          state->SetDownloadStatusAndNotify(origin, DOWNLOADS_NOT_ALLOWED);
          ret = false;
          std::move(callback).Run(false);
          break;
        }
        case CONTENT_SETTING_DEFAULT:
        case CONTENT_SETTING_ASK:
          state->PromptUserForDownload(std::move(callback), origin);
          state->increment_download_count();
          ret = false;
          break;
        case CONTENT_SETTING_SESSION_ONLY:
        case CONTENT_SETTING_NUM_SETTINGS:
        default:
          NOTREACHED_IN_MIGRATION();
          return;
      }
      break;
    }

    default:
      NOTREACHED_IN_MIGRATION();
  }

  if (!on_can_download_decided_callback_.is_null())
    on_can_download_decided_callback_.Run(ret);
}

void DownloadRequestLimiter::Remove(TabDownloadState* state,
                                    content::WebContents* contents) {
  DCHECK(base::Contains(state_map_, contents));
  state_map_.erase(contents);
  delete state;
}

void DownloadRequestLimiter::SetOnCanDownloadDecidedCallbackForTesting(
    CanDownloadDecidedCallback callback) {
  on_can_download_decided_callback_ = callback;
}
