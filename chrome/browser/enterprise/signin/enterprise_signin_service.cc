// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/enterprise_signin_service.h"

#include <string>

#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/enterprise/signin/enterprise_signin_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/sync/service/sync_service.h"
#include "google_apis/gaia/gaia_urls.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace enterprise_signin {

namespace {

bool IsNormalBrowserWithProfile(Browser* browser, Profile* profile) {
  return profile == browser->profile() && !browser->is_delete_scheduled() &&
         browser->type() == Browser::TYPE_NORMAL;
}

// Returns the Browser associated with `profile` that was most recently
// activated, or nullptr if none exists.
Browser* GetRecentBrowserForProfile(Profile* profile) {
  for (Browser* browser : BrowserList::GetInstance()->OrderedByActivation()) {
    // When tab switching, only look at same profile and anonymity level.
    if (IsNormalBrowserWithProfile(browser, profile)) {
      return browser;
    }
  }
  return nullptr;
}

// Returns true if `p` has at least one normal browser (that's not e.g. a PWA
// window). Doesn't count Incognito.
bool ProfileHasBrowser(Profile* p) {
  return base::ranges::any_of(*BrowserList::GetInstance(), [p](Browser* b) {
    return IsNormalBrowserWithProfile(b, p);
  });
}

}  // namespace

using TransportState = syncer::SyncService::TransportState;

EnterpriseSigninService::EnterpriseSigninService(Profile* profile)
    : profile_(profile) {
  CHECK(!profile->IsOffTheRecord());
  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kProfileReauthPrompt,
      base::BindRepeating(&EnterpriseSigninService::OnPrefChanged,
                          base::Unretained(this)));
  OnPrefChanged(prefs::kProfileReauthPrompt);
}

EnterpriseSigninService::~EnterpriseSigninService() = default;

void EnterpriseSigninService::OnStateChanged(syncer::SyncService* sync) {
  if (!ProfileHasBrowser(profile_.get())) {
    // For profiles that don't have browsers, don't open a new window just for
    // this.
    return;
  }

  TransportState new_transport_state = sync->GetTransportState();
  if (new_transport_state == last_transport_state_) {
    // Not a TransportState change, do nothing.
    return;
  }

  last_transport_state_ = new_transport_state;
  VLOG(2) << "TransportState changed: "
          << static_cast<int>(last_transport_state_);
  if (last_transport_state_ == TransportState::PAUSED) {
    OpenOrActivateGaiaReauthTab();
  }
}

void EnterpriseSigninService::OnSyncShutdown(syncer::SyncService* sync) {
  observation_.Reset();
}

void EnterpriseSigninService::OnPrefChanged(const std::string& pref_name) {
  ProfileReauthPrompt prompt_type = static_cast<ProfileReauthPrompt>(
      profile_->GetPrefs()->GetInteger(prefs::kProfileReauthPrompt));
  if (prompt_type == ProfileReauthPrompt::kPromptInTab) {
    syncer::SyncService* sync_service =
        SyncServiceFactory::GetInstance()->GetForProfile(profile_.get());
    if (!observation_.IsObservingSource(sync_service)) {
      observation_.Reset();
    }
    observation_.Observe(sync_service);
  } else {
    observation_.Reset();
  }
}

void EnterpriseSigninService::OpenOrActivateGaiaReauthTab() {
  if (Browser* browser = GetRecentBrowserForProfile(profile_.get())) {
    content::WebContents* tab =
        browser->tab_strip_model()->GetActiveWebContents();
    const GURL& tab_url = tab->GetVisibleURL();
    if (tab_url.SchemeIsHTTPOrHTTPS() &&
        GaiaUrls::GetInstance()->gaia_origin().IsSameOriginWith(tab_url)) {
      VLOG(2) << "Focused tab is a login page, nothing to do.";
    } else {
      VLOG(2) << "Focused tab is not a login page, opening a new one.";
      browser->command_controller()->ExecuteCommandWithDisposition(
          IDC_SHOW_SIGNIN_WHEN_PAUSED,
          WindowOpenDisposition::NEW_FOREGROUND_TAB);
    }
  }
}

}  // namespace enterprise_signin
