// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/captive_portal/captive_portal_tab_reloader.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/captive_portal/captive_portal_service.h"
#include "chrome/browser/captive_portal/captive_portal_service_factory.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"

using captive_portal::CaptivePortalResult;

namespace {

// The time to wait for a slow loading SSL page before triggering a captive
// portal check.
const int kDefaultSlowSSLTimeSeconds = 30;

// Returns true if an SSL request resulting in |error| may indicate a captive
// portal.
bool SslNetErrorMayImplyCaptivePortal(int error) {
  // May be returned when a captive portal silently blocks an SSL request.
  if (error == net::ERR_CONNECTION_TIMED_OUT)
    return true;

  // May be returned when a captive portal lets SSL requests connect, but
  // disconnects Chrome after Chrome starts SSL negotiation, or sends an
  // HTTP response.
  if (error == net::ERR_SSL_PROTOCOL_ERROR)
    return true;

  return net::IsCertificateError(error);
}

}  // namespace

CaptivePortalTabReloader::CaptivePortalTabReloader(
    Profile* profile,
    content::WebContents* web_contents,
    const OpenLoginTabCallback& open_login_tab_callback)
    : profile_(profile),
      web_contents_(web_contents),
      state_(STATE_NONE),
      provisional_main_frame_load_(false),
      ssl_url_in_redirect_chain_(false),
      slow_ssl_load_time_(
          base::TimeDelta::FromSeconds(kDefaultSlowSSLTimeSeconds)),
      open_login_tab_callback_(open_login_tab_callback) {}

CaptivePortalTabReloader::~CaptivePortalTabReloader() {
}

void CaptivePortalTabReloader::OnLoadStart(bool is_ssl) {
  provisional_main_frame_load_ = true;
  ssl_url_in_redirect_chain_ = is_ssl;

  SetState(STATE_NONE);

  // Start the slow load timer for SSL pages.
  // TODO(mmenke):  Should this look at the port instead?  The reason the
  //                request never connects is because of the port, not the
  //                protocol.
  if (is_ssl)
    SetState(STATE_TIMER_RUNNING);
}

void CaptivePortalTabReloader::OnLoadCommitted(int net_error) {
  provisional_main_frame_load_ = false;
  ssl_url_in_redirect_chain_ = false;

  if (state_ == STATE_NONE)
    return;

  // If |net_error| is not an error code that could indicate there's a captive
  // portal, reset the state.
  if (!SslNetErrorMayImplyCaptivePortal(net_error)) {
    // TODO(mmenke):  If the new URL is the same as the old broken URL, and the
    //                request succeeds, should probably trigger another
    //                captive portal check.
    SetState(STATE_NONE);
    return;
  }

  // The page returned an error out before the timer triggered.  Go ahead and
  // try to detect a portal now, rather than waiting for the timer.
  if (state_ == STATE_TIMER_RUNNING) {
    OnSlowSSLConnect();
    return;
  }

  // If the tab needs to reload, do so asynchronously, to avoid reentrancy
  // issues.
  if (state_ == STATE_NEEDS_RELOAD) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&CaptivePortalTabReloader::ReloadTabIfNeeded,
                                  weak_factory_.GetWeakPtr()));
  }
}

void CaptivePortalTabReloader::OnAbort() {
  provisional_main_frame_load_ = false;
  ssl_url_in_redirect_chain_ = false;

  SetState(STATE_NONE);
}

void CaptivePortalTabReloader::OnRedirect(bool is_ssl) {
  SetState(STATE_NONE);
  if (!is_ssl)
    return;
  // Only start the SSL timer running if no SSL URL has been seen in the current
  // redirect chain.  If we've already successfully downloaded one SSL URL,
  // assume we're not behind a captive portal.
  if (!ssl_url_in_redirect_chain_)
    SetState(STATE_TIMER_RUNNING);
  ssl_url_in_redirect_chain_ = true;
}

void CaptivePortalTabReloader::OnCaptivePortalResults(
    CaptivePortalResult previous_result,
    CaptivePortalResult result) {
  if (result == captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL) {
    if (state_ == STATE_MAYBE_BROKEN_BY_PORTAL) {
      SetState(STATE_BROKEN_BY_PORTAL);
      MaybeOpenCaptivePortalLoginTab();
    }
    return;
  }

  switch (state_) {
    case STATE_MAYBE_BROKEN_BY_PORTAL:
    case STATE_TIMER_RUNNING:
      // If the previous result was BEHIND_CAPTIVE_PORTAL, and the state is
      // either STATE_MAYBE_BROKEN_BY_PORTAL or STATE_TIMER_RUNNING, reload the
      // tab.  In the latter case, the tab has yet to commit, but is an SSL
      // page, so if the page ends up at an error caused by a captive portal, it
      // will be reloaded.  If not, the state will just be reset.  The helps in
      // the case that a user tries to reload a tab, and then quickly logs in.
      if (previous_result == captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL) {
        SetState(STATE_NEEDS_RELOAD);
        return;
      }
      SetState(STATE_NONE);
      return;

    case STATE_BROKEN_BY_PORTAL:
      // Either reload the tab now, if an error page has already been committed
      // or an interstitial is being displayed, or reload it if and when a
      // timeout commits.
      SetState(STATE_NEEDS_RELOAD);
      return;

    case STATE_NEEDS_RELOAD:
    case STATE_NONE:
      // If the tab needs to reload or is in STATE_NONE, do nothing.  The reload
      // case shouldn't be very common, since it only lasts until a tab times
      // out, but it's still possible.
      return;

    default:
      NOTREACHED();
  }
}

void CaptivePortalTabReloader::OnSSLCertError(const net::SSLInfo& ssl_info) {
  // TODO(mmenke):  Figure out if any cert errors should be ignored.  The
  // most common errors when behind captive portals are likely
  // ERR_CERT_COMMON_NAME_INVALID and ERR_CERT_AUTHORITY_INVALID.  It's unclear
  // if captive portals cause any others.
  if (state_ == STATE_TIMER_RUNNING)
    SetState(STATE_MAYBE_BROKEN_BY_PORTAL);
}

void CaptivePortalTabReloader::OnSlowSSLConnect() {
  SetState(STATE_MAYBE_BROKEN_BY_PORTAL);
}

void CaptivePortalTabReloader::SetState(State new_state) {
  // Stop the timer even when old and new states are the same.
  if (state_ == STATE_TIMER_RUNNING) {
    slow_ssl_load_timer_.Stop();
  } else {
    DCHECK(!slow_ssl_load_timer_.IsRunning());
  }

  // Check for unexpected state transitions.
  switch (state_) {
    case STATE_NONE:
      DCHECK(new_state == STATE_NONE ||
             new_state == STATE_TIMER_RUNNING);
      break;
    case STATE_TIMER_RUNNING:
      DCHECK(new_state == STATE_NONE ||
             new_state == STATE_MAYBE_BROKEN_BY_PORTAL ||
             new_state == STATE_NEEDS_RELOAD);
      break;
    case STATE_MAYBE_BROKEN_BY_PORTAL:
      DCHECK(new_state == STATE_NONE ||
             new_state == STATE_BROKEN_BY_PORTAL ||
             new_state == STATE_NEEDS_RELOAD);
      break;
    case STATE_BROKEN_BY_PORTAL:
      DCHECK(new_state == STATE_NONE ||
             new_state == STATE_NEEDS_RELOAD);
      break;
    case STATE_NEEDS_RELOAD:
      DCHECK_EQ(STATE_NONE, new_state);
      break;
    default:
      NOTREACHED();
      break;
  }

  state_ = new_state;

  switch (state_) {
    case STATE_TIMER_RUNNING:
      slow_ssl_load_timer_.Start(
          FROM_HERE,
          slow_ssl_load_time_,
          this,
          &CaptivePortalTabReloader::OnSlowSSLConnect);
      break;

    case STATE_MAYBE_BROKEN_BY_PORTAL:
      CheckForCaptivePortal();
      break;

    case STATE_NEEDS_RELOAD:
      // Try to reload the tab now.
      ReloadTabIfNeeded();
      break;

    default:
      break;
  }
}

void CaptivePortalTabReloader::ReloadTabIfNeeded() {
  // If the page no longer needs to be reloaded, do nothing.
  if (state_ != STATE_NEEDS_RELOAD)
    return;

  // If there's still a provisional load going, do nothing unless there's an
  // interstitial page.  Reloading when displaying an interstitial page will
  // reload the underlying page, even if it hasn't yet committed.
  if (provisional_main_frame_load_ &&
      !content::InterstitialPage::GetInterstitialPage(web_contents_)) {
    return;
  }

  SetState(STATE_NONE);
  ReloadTab();
}

void CaptivePortalTabReloader::ReloadTab() {
  content::NavigationController* controller = &web_contents_->GetController();
  if (controller->GetLastCommittedEntry() &&
      !controller->GetLastCommittedEntry()->GetHasPostData()) {
    controller->Reload(content::ReloadType::NORMAL, true);
  }
}

void CaptivePortalTabReloader::MaybeOpenCaptivePortalLoginTab() {
  open_login_tab_callback_.Run();
}

void CaptivePortalTabReloader::CheckForCaptivePortal() {
  CaptivePortalService* service =
      CaptivePortalServiceFactory::GetForProfile(profile_);
  if (service)
    service->DetectCaptivePortal();
}
