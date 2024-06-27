// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/network/network_portal_signin_window.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/pref_names.h"
#include "components/device_event_log/device_event_log.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_prefs.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/network_change.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif

namespace chromeos {

namespace {

Profile* GetOTROrActiveProfile() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  DCHECK(profile);

  // In Guest mode, the active profile is OTR. Since we do not support creating
  // an OTR profile from another OTR profile we use the active profile for
  // captive portal signin.
  if (profile->IsOffTheRecord()) {
    return profile;
  }

  // When not in Guest mode we use a separate signin OTR profile to avoid
  // passing existing OTR cookies to the captive portal signin page, see
  // b/245578628 for details.
  static base::NoDestructor<Profile::OTRProfileID> otr_profile_id(
      Profile::OTRProfileID::CreateUniqueForCaptivePortal());
  Profile* otr_profile =
      profile->GetOffTheRecordProfile(*otr_profile_id,
                                      /*create_if_needed=*/true);
  DCHECK(otr_profile);
  return otr_profile;
}

}  // namespace

// static
NetworkPortalSigninWindow* NetworkPortalSigninWindow::Get() {
  static base::NoDestructor<NetworkPortalSigninWindow> instance;
  return instance.get();
}

NetworkPortalSigninWindow::NetworkPortalSigninWindow() = default;

NetworkPortalSigninWindow::~NetworkPortalSigninWindow() = default;

void NetworkPortalSigninWindow::Show(const GURL& url) {
  Profile* profile = GetOTROrActiveProfile();

  Browser* browser = chrome::FindBrowserWithID(window_session_id_);
  if (browser) {
    NET_LOG(EVENT) << "Show existing portal signin window";
    NavigateParams params(browser, url, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
    params.window_action = NavigateParams::SHOW_WINDOW;
    params.user_gesture = true;
    params.trusted_source = false;
    ::Navigate(&params);
    return;
  }

  NET_LOG(EVENT) << "Show new portal signin window";
  NavigateParams params(profile, url, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = WindowOpenDisposition::NEW_POPUP;
  params.user_gesture = true;
  params.trusted_source = false;
  // `captive_portal_window_type = kPopup` is used on desktop Chrome to identify
  // captive portal signin popup windows. This affects the following behaviors:
  // * Secure DNS is disabled in ChromeContentBrowserClient
  // * The window title is customized in Browser
  params.captive_portal_window_type =
      captive_portal::CaptivePortalWindowType::kPopup;
  auto handle = ::Navigate(&params);
  if (!handle) {
    NET_LOG(ERROR) << "Failed to navigate to captive portal url: " << url;
    window_session_id_ = SessionID::InvalidValue();
    return;
  }
  window_session_id_ = params.browser->session_id();
  window_observer_ =
      std::make_unique<WindowObserver>(handle->GetWebContents(), this);
}

Browser* NetworkPortalSigninWindow::GetBrowserForTesting() {
  return chrome::FindBrowserWithID(window_session_id_);
}

class NetworkPortalSigninWindow::WindowObserver
    : public content::WebContentsObserver {
 public:
  WindowObserver(content::WebContents* web_contents,
                 NetworkPortalSigninWindow* controller)
      : content::WebContentsObserver(web_contents), controller_(controller) {}
  ~WindowObserver() override = default;

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    RequestPortalDetection();
  }

  void WebContentsDestroyed() override { RequestPortalDetection(); }

 private:
  void RequestPortalDetection() {
    NET_LOG(EVENT) << "Request portal detection";
    controller_->portal_detection_requested_for_testing_++;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    ash::NetworkHandler::Get()
        ->network_state_handler()
        ->RequestPortalDetection();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
    chromeos::LacrosService::Get()
        ->GetRemote<crosapi::mojom::NetworkChange>()
        ->RequestPortalDetection();
#endif
  }

 private:
  raw_ptr<NetworkPortalSigninWindow> controller_;
};

content::WebContents* NetworkPortalSigninWindow::GetWebContentsForTesting() {
  if (!window_observer_.get()) {
    return nullptr;
  }
  return window_observer_->web_contents();
}

}  // namespace chromeos
