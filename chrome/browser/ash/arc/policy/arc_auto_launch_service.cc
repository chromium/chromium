// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/policy/arc_auto_launch_service.h"

#include <string_view>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/ash/app_list/arc/arc_app_launcher.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/experiences/arc/arc_browser_context_keyed_service_factory_base.h"
#include "chromeos/ash/experiences/arc/arc_prefs.h"
#include "components/prefs/pref_service.h"
#include "ui/display/types/display_constants.h"

namespace arc {

namespace {

// Singleton factory for ArcAutoLaunchService.
class ArcAutoLaunchServiceFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcAutoLaunchService,
          ArcAutoLaunchServiceFactory> {
 public:
  static constexpr const char* kName = "ArcAutoLaunchServiceFactory";

  static ArcAutoLaunchServiceFactory* GetInstance() {
    static base::NoDestructor<ArcAutoLaunchServiceFactory> instance;
    return instance.get();
  }

 private:
  friend base::NoDestructor<ArcAutoLaunchServiceFactory>;
  ArcAutoLaunchServiceFactory() {
    DependsOn(ArcAppListPrefsFactory::GetInstance());
  }
  ~ArcAutoLaunchServiceFactory() override = default;
};

// Returns the first package listed in the AutoLaunchAndroidApps policy, or an
// empty string if there is none. AutoLaunchAndroidAppsPolicyHandler only lets
// valid package names through to the pref.
std::string GetConfiguredPackage(const PrefService& pref_service) {
  const base::ListValue& packages =
      pref_service.GetList(prefs::kArcAutoLaunchApps);
  return packages.empty() ? std::string() : packages.front().GetString();
}

// Returns the ID of the app to launch for `package_name`, or an empty string if
// the package has no launchable activity. If there are several, the activity
// whose name sorts first is picked, so that the same activity is launched on
// every sign-in.
std::string GetLaunchableAppId(const ArcAppListPrefs& app_list_prefs,
                               const std::string& package_name) {
  std::string app_id_to_launch;
  std::string activity_to_launch;
  for (const std::string& app_id :
       app_list_prefs.GetAppsForPackage(package_name)) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
        app_list_prefs.GetApp(app_id);
    if (!app_info || !app_info->launchable) {
      continue;
    }
    if (app_id_to_launch.empty() || app_info->activity < activity_to_launch) {
      app_id_to_launch = app_id;
      activity_to_launch = app_info->activity;
    }
  }
  return app_id_to_launch;
}

// Returns true if ARC is enabled. The service is only created for profiles
// where ARC is allowed, so this only depends on the user or policy setting.
bool IsArcEnabled(const PrefService& pref_service) {
  return pref_service.GetBoolean(prefs::kArcEnabled);
}

// Describes why `package_name` can't be launched, for logging.
std::string_view GetLaunchFailureReason(const PrefService& pref_service,
                                        const ArcAppListPrefs& app_list_prefs,
                                        const std::string& package_name) {
  // IsPackageInstalled() also returns false while ARC is disabled, so check
  // that first.
  if (!IsArcEnabled(pref_service)) {
    return "ARC is disabled";
  }
  if (!app_list_prefs.IsPackageInstalled(package_name)) {
    return "the package is not installed";
  }
  return "the package has no launchable activity";
}

}  // namespace

// static
ArcAutoLaunchService* ArcAutoLaunchService::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcAutoLaunchServiceFactory::GetForBrowserContext(context);
}

// static
ArcAutoLaunchService* ArcAutoLaunchService::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcAutoLaunchServiceFactory::GetForBrowserContextForTesting(context);
}

// static
void ArcAutoLaunchService::EnsureFactoryBuilt() {
  ArcAutoLaunchServiceFactory::GetInstance();
}

ArcAutoLaunchService::ArcAutoLaunchService(content::BrowserContext* context,
                                           ArcBridgeService* bridge_service)
    : profile_(Profile::FromBrowserContext(context)) {
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kArcAutoLaunchApps,
      base::BindRepeating(&ArcAutoLaunchService::OnPolicyChanged,
                          base::Unretained(this)));

  StartLaunch(GetConfiguredPackage(*profile_->GetPrefs()));
}

ArcAutoLaunchService::~ArcAutoLaunchService() = default;

void ArcAutoLaunchService::Shutdown() {
  pref_change_registrar_.Reset();
  StopLaunch();
}

void ArcAutoLaunchService::OnAppRegistered(
    const std::string& app_id,
    const ArcAppListPrefs::AppInfo& app_info) {
  MaybeLaunchAppAfterUpdate(app_info);
}

void ArcAutoLaunchService::OnAppStatesChanged(
    const std::string& app_id,
    const ArcAppListPrefs::AppInfo& app_info) {
  // An activity that is already registered, e.g. as a non-launchable task
  // entry, reports becoming launchable here instead of in OnAppRegistered().
  MaybeLaunchAppAfterUpdate(app_info);
}

void ArcAutoLaunchService::OnArcAppListPrefsDestroyed() {
  StopLaunch();
}

void ArcAutoLaunchService::StartLaunch(const std::string& package_name) {
  if (package_name.empty()) {
    return;
  }
  package_name_ = package_name;

  // The app might not be installed yet, e.g. on first sign-in, so wait for it.
  arc_app_list_prefs_observation_.Observe(
      &CHECK_DEREF(ArcAppListPrefs::Get(profile_)));
  timeout_timer_.Start(FROM_HERE, kLaunchTimeout, this,
                       &ArcAutoLaunchService::OnTimeout);
  MaybeLaunchApp();
}

void ArcAutoLaunchService::MaybeLaunchApp() {
  // Nothing to do if the app is already being launched, or if launching was
  // stopped.
  if (app_launcher_ || package_name_.empty()) {
    return;
  }
  const ArcAppListPrefs& app_list_prefs =
      CHECK_DEREF(ArcAppListPrefs::Get(profile_));
  const std::string app_id =
      IsArcEnabled(*profile_->GetPrefs())
          ? GetLaunchableAppId(app_list_prefs, package_name_)
          : std::string();
  if (app_id.empty()) {
    // Keep waiting until the app is installed or the timeout fires.
    return;
  }

  VLOG(1) << "Waiting to auto-launch " << package_name_;
  // The app is installed, so there is no need to wait for it anymore.
  arc_app_list_prefs_observation_.Reset();
  // ArcAppLauncher launches the app as soon as it is ready.
  app_launcher_ = std::make_unique<ArcAppLauncher>(
      profile_, app_id, /*launch_intent=*/nullptr,
      /*deferred_launch_allowed=*/false, display::kInvalidDisplayId,
      apps::LaunchSource::kFromChromeInternal);
}

void ArcAutoLaunchService::MaybeLaunchAppAfterUpdate(
    const ArcAppListPrefs::AppInfo& app_info) {
  if (app_info.package_name != package_name_) {
    return;
  }
  // The activities of a package are updated one at a time. Wait until the
  // current batch is processed, so that the launched activity doesn't depend
  // on the update order.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ArcAutoLaunchService::MaybeLaunchApp,
                                weak_ptr_factory_.GetWeakPtr()));
}

void ArcAutoLaunchService::StopLaunch() {
  timeout_timer_.Stop();
  arc_app_list_prefs_observation_.Reset();
  app_launcher_.reset();
  package_name_.clear();
}

bool ArcAutoLaunchService::HasLaunchedApp() const {
  return app_launcher_ && app_launcher_->app_launched();
}

void ArcAutoLaunchService::OnTimeout() {
  if (HasLaunchedApp()) {
    return;
  }
  std::string_view reason = "the app did not become ready";
  if (!app_launcher_) {
    reason = GetLaunchFailureReason(*profile_->GetPrefs(),
                                    CHECK_DEREF(ArcAppListPrefs::Get(profile_)),
                                    package_name_);
  }
  LOG(WARNING) << "Timed out auto-launching " << package_name_ << ": "
               << reason;
  StopLaunch();
}

void ArcAutoLaunchService::OnPolicyChanged() {
  // At most one app is auto-launched per session.
  if (HasLaunchedApp()) {
    return;
  }
  const std::string package_name = GetConfiguredPackage(*profile_->GetPrefs());
  if (package_name == package_name_) {
    return;
  }
  StopLaunch();
  StartLaunch(package_name);
}

}  // namespace arc
