// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_tab_observer.h"

#include "base/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/buildflags.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "chrome/browser/safe_browsing/client_side_detection_host_delegate.h"
#include "chrome/browser/safe_browsing/client_side_detection_service_factory.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/safe_browsing/content/browser/client_side_detection_host.h"
#include "components/safe_browsing/content/browser/client_side_detection_service.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#endif

namespace safe_browsing {

#if !BUILDFLAG(SAFE_BROWSING_AVAILABLE)
// Provide a dummy implementation so that
// std::unique_ptr<ClientSideDetectionHost>
// has a concrete destructor to call. This is necessary because it is used
// as a member of SafeBrowsingTabObserver, even if it only ever contains NULL.
// TODO(shess): This is weird, why not just guard the instance variable?
class ClientSideDetectionHost { };
#endif

SafeBrowsingTabObserver::SafeBrowsingTabObserver(
    content::WebContents* web_contents) : web_contents_(web_contents) {
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  PrefService* prefs = profile->GetPrefs();
  if (prefs) {
    pref_change_registrar_.Init(prefs);
    pref_change_registrar_.Add(
        prefs::kSafeBrowsingEnabled,
        base::BindRepeating(
            &SafeBrowsingTabObserver::UpdateSafebrowsingDetectionHost,
            base::Unretained(this)));

    ClientSideDetectionService* csd_service =
        ClientSideDetectionServiceFactory::GetForProfile(profile);
    if (IsSafeBrowsingEnabled(*prefs) &&
        g_browser_process->safe_browsing_service() && csd_service) {
      safebrowsing_detection_host_ =
          ClientSideDetectionHostDelegate::CreateHost(web_contents);
      csd_service->AddClientSideDetectionHost(
          safebrowsing_detection_host_.get());
    }
  }
#endif
}

SafeBrowsingTabObserver::~SafeBrowsingTabObserver() {
}

////////////////////////////////////////////////////////////////////////////////
// Internal helpers

void SafeBrowsingTabObserver::UpdateSafebrowsingDetectionHost() {
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  PrefService* prefs = profile->GetPrefs();
  bool safe_browsing = IsSafeBrowsingEnabled(*prefs);
  ClientSideDetectionService* csd_service =
      ClientSideDetectionServiceFactory::GetForProfile(profile);
  if (safe_browsing && csd_service) {
    if (!safebrowsing_detection_host_.get()) {
      safebrowsing_detection_host_ =
          ClientSideDetectionHostDelegate::CreateHost(web_contents_);
      csd_service->AddClientSideDetectionHost(
          safebrowsing_detection_host_.get());
    }
  } else {
    safebrowsing_detection_host_.reset();
  }
#endif
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SafeBrowsingTabObserver)

}  // namespace safe_browsing
