// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/arc/arc_fast_app_reinstall_starter.h"

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/arc_util.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"

namespace arc {

// TODO(rsgingerrs): This shares a lot of functionality with ArcPaiStarter.
// Should create a base class and put common code there.
ArcFastAppReinstallStarter::ArcFastAppReinstallStarter(
    content::BrowserContext* context,
    PrefService* pref_service)
    : context_(context), pref_service_(pref_service) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(context_);
  // Prefs may not available in some unit tests.
  if (!prefs)
    return;
  prefs->AddObserver(this);
  MaybeStartFastAppReinstall();
}

ArcFastAppReinstallStarter::~ArcFastAppReinstallStarter() {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(context_);
  if (!prefs)
    return;
  prefs->RemoveObserver(this);
}

// static
std::unique_ptr<ArcFastAppReinstallStarter>
ArcFastAppReinstallStarter::CreateIfNeeded(content::BrowserContext* context,
                                           PrefService* pref_service) {
  if (pref_service->GetBoolean(prefs::kArcFastAppReinstallStarted))
    return nullptr;
  return std::make_unique<ArcFastAppReinstallStarter>(context, pref_service);
}

void ArcFastAppReinstallStarter::OnAppsSelectionFinished() {
  MaybeStartFastAppReinstall();
}

void ArcFastAppReinstallStarter::MaybeStartFastAppReinstall() {
  if (started_) {
    VLOG(2) << "Fast App Reinstall has already started.";
    return;
  }

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(context_);
  DCHECK(prefs);
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      prefs->GetApp(kPlayStoreAppId);
  if (!app_info || !app_info->ready) {
    VLOG(2) << "Play Store is not ready. Will not start Fast App Reinstall.";
    return;
  }

  const std::vector<std::string> selected_packages =
      GetSelectedPackagesFromPrefs(context_);
  if (selected_packages.size() <= 0) {
    VLOG(2) << "No selected packages. Will not start Fast App Reinstall.";
    return;
  }

  VLOG(2) << "Fast App Reinstall started...";
  started_ = true;
  StartFastAppReinstallFlow(selected_packages);
  pref_service_->SetBoolean(prefs::kArcFastAppReinstallStarted, true);
}

void ArcFastAppReinstallStarter::OnAppRegistered(
    const std::string& app_id,
    const ArcAppListPrefs::AppInfo& app_info) {
  OnAppStatesChanged(app_id, app_info);
}

void ArcFastAppReinstallStarter::OnAppStatesChanged(
    const std::string& app_id,
    const ArcAppListPrefs::AppInfo& app_info) {
  if (app_id == kPlayStoreAppId && app_info.ready)
    MaybeStartFastAppReinstall();
}

}  // namespace arc
