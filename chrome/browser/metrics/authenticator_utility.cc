// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/authenticator_utility.h"

#include <memory>

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/is_uvpaa.h"

#if defined(OS_MAC)
#include "device/fido/mac/authenticator.h"
#endif

#if defined(OS_WIN)
#include "device/fido/win/webauthn_api.h"
#endif

namespace authenticator_utility {

void ReportAvailability(bool available) {
  base::UmaHistogramBoolean(
      "WebAuthentication.IsUVPlatformAuthenticatorAvailable", available);
}

#if defined(OS_MAC)
void ReportUVPlatformAuthenticatorAvailabilityWithConfig(
    base::Optional<device::fido::mac::AuthenticatorConfig> config) {
  ReportAvailability(config &&
                     content::IsUVPlatformAuthenticatorAvailable(*config));
}

void ReportUVPlatformAuthenticatorAvailabilityMainThreadMac() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // Tests can shut down before this task is run.
  if (!g_browser_process)
    return;

  // Startup metrics are recording during PostBrowserStart() which is after
  // profile initialization. However some tests run PostBrowserStart() without
  // setting up profiles so there still needs to be a guard.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager || !g_browser_process->local_state()) {
    return;
  }
  Profile* profile = profile_manager->GetProfileByPath(
      profile_manager->GetLastUsedProfileDir(profile_manager->user_data_dir()));
  // Some tests have profiles but do not load the last profile before
  // PostBrowserStart().
  if (!profile) {
    return;
  }

  // Return to a low-priority thread for the actual check.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ReportUVPlatformAuthenticatorAvailabilityWithConfig,
                     ChromeAuthenticatorRequestDelegate::
                         TouchIdAuthenticatorConfigForProfile(profile)));
}
#endif

void ReportUVPlatformAuthenticatorAvailability() {
  // This only reports metrics for desktop platforms. For mobile devices, the
  // platform version is an exact proxy for whether a platform authenticator
  // can be used.
#if defined(OS_MAC)
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // Getting the profile has to be done on the main thread to avoid race
  // conditions.
  content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostTask(FROM_HERE,
                 base::BindOnce(
                     &ReportUVPlatformAuthenticatorAvailabilityMainThreadMac));
#elif defined(OS_WIN)
  std::unique_ptr<content::AuthenticatorRequestClientDelegate> client_delegate =
      std::make_unique<content::AuthenticatorRequestClientDelegate>();
  device::WinWebAuthnApi* win_webauthn_api =
      device::WinWebAuthnApi::GetDefault();
  ReportAvailability(
      win_webauthn_api &&
      content::IsUVPlatformAuthenticatorAvailable(win_webauthn_api));
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  ReportAvailability(content::IsUVPlatformAuthenticatorAvailable());
#endif
}

}  // namespace authenticator_utility
