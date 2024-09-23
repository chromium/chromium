// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_install_url_handler.h"

#include "chrome/browser/ash/borealis/borealis_app_launcher.h"
#include "chrome/browser/ash/borealis/borealis_installer.h"
#include "chrome/browser/ash/borealis/borealis_prefs.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_service_factory.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace borealis {

namespace {

// Whether this URL is chromeos-steam://install.
bool IsInstallUrl(const GURL& url) {
  return url.is_valid() && url == "chromeos-steam://install";
}

// Handles chromeos-steam://install URLs.
void HandleInstallUrl(base::WeakPtr<Profile> profile, GURL url) {
  DCHECK(IsInstallUrl(url));
  if (!profile) {
    return;
  }
  BorealisServiceFactory::GetForProfile(profile.get())
      ->AppLauncher()
      .Launch(kClientAppId, borealis::BorealisLaunchSource::kInstallUrl,
              base::DoNothing());
}
}  // namespace

BorealisInstallUrlHandler::BorealisInstallUrlHandler(Profile* profile)
    : profile_(profile) {
  RegisterHandler();
}

void BorealisInstallUrlHandler::RegisterHandler() {
  auto* registry_service =
      guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile_);
  if (!registry_service) {
    return;
  }

  registry_service->RegisterTransientUrlHandler(
      /*handler=*/guest_os::GuestOsUrlHandler(
          l10n_util::GetStringUTF8(IDS_BOREALIS_INSTALLER_APP_NAME),
          base::BindRepeating([](Profile* profile, const GURL& url) {
            // Show the UI in a posted task instead of immediately.
            // Without this, the current click event reliably returns
            // focus to the Ash browser window, leaving our new UI in the
            // background.
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(&HandleInstallUrl, profile->GetWeakPtr(), url));
          })),
      /*canHandleCallback=*/base::BindRepeating(IsInstallUrl));
}

}  // namespace borealis
