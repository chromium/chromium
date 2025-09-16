// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/arcvm_app/kiosk_arcvm_app_service.h"

#include <memory>
#include <optional>
#include <string>
#include <unordered_set>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/app_mode/arcvm_app/kiosk_arcvm_app_data.h"
#include "chrome/browser/ash/app_mode/arcvm_app/kiosk_arcvm_app_manager.h"
#include "chrome/browser/ash/app_mode/arcvm_app/kiosk_arcvm_app_service_factory.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace ash {

namespace {

constexpr size_t kNonComplianceReasonAppNotInstalled = 5;

}  // namespace

// static
KioskArcvmAppService* KioskArcvmAppService::Create(Profile* profile) {
  return new KioskArcvmAppService(profile);
}

// static
KioskArcvmAppService* KioskArcvmAppService::Get(
    content::BrowserContext* context) {
  return KioskArcvmAppServiceFactory::GetForBrowserContext(context);
}

void KioskArcvmAppService::AddObserver(KioskAppLauncher::Observer* observer) {
  observers_.AddObserver(observer);
}

void KioskArcvmAppService::RemoveObserver(
    KioskAppLauncher::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void KioskArcvmAppService::Shutdown() {
  // TODO(crbug.com/418637197): Use ScopedObservation for each of the following.
  ArcAppListPrefs::Get(profile_)->RemoveObserver(this);
  // It can be null unittests.
  if (arc::ArcSessionManager::Get()) {
    arc::ArcSessionManager::Get()->RemoveObserver(this);
  }
  app_manager_->RemoveObserver(this);
  arc::ArcPolicyBridge::GetForBrowserContext(profile_)->RemoveObserver(this);
}

void KioskArcvmAppService::OnAppRegistered(
    const std::string& app_id,
    const ArcAppListPrefs::AppInfo& app_info) {
  if (!app_id_.empty() && app_id != app_id_) {
    return;
  }
  PreconditionsChanged();
}

void KioskArcvmAppService::OnAppStatesChanged(
    const std::string& app_id,
    const ArcAppListPrefs::AppInfo& app_info) {
  if (!app_id_.empty() && app_id != app_id_) {
    return;
  }
  PreconditionsChanged();
}

void KioskArcvmAppService::OnPackageListInitialRefreshed() {
  // The app could already be registered.
  PreconditionsChanged();
}

void KioskArcvmAppService::OnKioskAppsSettingsChanged() {
  PreconditionsChanged();
}

void KioskArcvmAppService::OnTaskCreated(int32_t task_id,
                                         const std::string& package_name,
                                         const std::string& activity,
                                         const std::string& intent,
                                         int32_t session_id) {
  // Store task id of the app to stop it later when needed.
  if (app_info_ && package_name == app_info_->package_name &&
      activity == app_info_->activity) {
    task_id_ = std::make_optional(task_id);
    observers_.NotifyAppLaunched();
  }
}

void KioskArcvmAppService::OnTaskDestroyed(int32_t task_id) {
  if (task_id_ == task_id) {
    ResetAppLauncher();
    // Trying to restart app if it was somehow closed or crashed
    // as kiosk app should always be running during the session.
    PreconditionsChanged();
  }
}

void KioskArcvmAppService::OnAppWindowLaunched() {
  observers_.NotifyAppWindowCreated();
}

void KioskArcvmAppService::OnIconUpdated(ArcAppIcon* icon) {
  CHECK_EQ(icon, app_icon_.get());
  if (icon->image_skia().isNull()) {
    app_icon_.release();
    return;
  }
  // TODO(crbug.com/418871771): Refactor to ash::AnnotatedAccountId::Get
  AccountId account_id = multi_user_util::GetAccountIdFromProfile(profile_);
  app_manager_->UpdateNameAndIcon(account_id, app_info_->name,
                                  app_icon_->image_skia());
  observers_.NotifyAppDataUpdated();
}

void KioskArcvmAppService::OnArcSessionRestarting() {
  // Reset state as the app is for sure not running.
  VLOG(2) << "Clearing ARC Kiosk state on restart";
  ResetAppLauncher();
}

void KioskArcvmAppService::OnArcSessionStopped(arc::ArcStopReason reason) {
  // Reset state as the app is for sure not running.
  VLOG(2) << "Clearing ARC Kiosk state on stop";
  ResetAppLauncher();
}

void KioskArcvmAppService::OnComplianceReportReceived(
    const base::Value* compliance_report) {
  VLOG(2) << "Compliance report received";
  compliance_report_received_ = true;
  pending_policy_app_installs_.clear();
  const base::Value::List* const details =
      compliance_report->GetDict().FindList("nonComplianceDetails");
  if (!details) {
    PreconditionsChanged();
    return;
  }

  for (const auto& detail : *details) {
    const base::Value::Dict& detail_dict = detail.GetDict();
    std::optional<size_t> reason = detail_dict.FindInt("nonComplianceReason");
    if (reason != kNonComplianceReasonAppNotInstalled) {
      continue;
    }
    const std::string* const app_name = detail_dict.FindString("packageName");
    if (!app_name || app_name->empty()) {
      continue;
    }
    pending_policy_app_installs_.insert(*app_name);
  }
  PreconditionsChanged();
}

KioskArcvmAppService::KioskArcvmAppService(Profile* profile)
    : profile_(profile),
      app_manager_(CHECK_DEREF(KioskArcvmAppManager::Get())) {
  ArcAppListPrefs::Get(profile_)->AddObserver(this);
  arc::ArcSessionManager::Get()->AddObserver(this);
  app_manager_->AddObserver(this);
  arc::ArcPolicyBridge::GetForBrowserContext(profile_)->AddObserver(this);
  PreconditionsChanged();
}

KioskArcvmAppService::~KioskArcvmAppService() = default;

void KioskArcvmAppService::RequestIconUpdate() {
  // Request only once when app_icon_ is not initialized.
  if (!app_info_ || !app_info_->ready || app_icon_) {
    return;
  }
  app_icon_ = std::make_unique<ArcAppIcon>(
      profile_, app_id_,
      SharedAppListConfig::instance().default_grid_icon_dimension(), this);
  app_icon_->image_skia().GetRepresentation(ui::GetSupportedResourceScaleFactor(
      display::Screen::Get()->GetPrimaryDisplay().device_scale_factor()));
  // Apply default image now and in case icon is updated then OnIconUpdated()
  // will be called additionally.
  OnIconUpdated(app_icon_.get());
}

void KioskArcvmAppService::PreconditionsChanged() {
  VLOG(2) << "Preconditions for kiosk app changed";
  app_id_ = GetAppId();
  if (app_id_.empty()) {
    VLOG(2) << "Kiosk app is not available";
    return;
  }
  app_info_ = ArcAppListPrefs::Get(profile_)->GetApp(app_id_);
  VLOG_IF(2, app_info_ && app_info_->ready) << "Kiosk app is ready";
  VLOG(2) << "Policy compliance is "
          << (compliance_report_received_ ? "reported" : "not yet reported");
  VLOG(2) << "Kiosk app with id: " << app_id_ << " is "
          << (app_launcher_ ? "already launched" : "not yet launched");
  VLOG(2) << "Kiosk app is policy "
          << (pending_policy_app_installs_.count(app_info_->package_name)
                  ? "non-compliant"
                  : "compliant");
  RequestIconUpdate();
  if (app_info_ && app_info_->ready && compliance_report_received_ &&
      pending_policy_app_installs_.count(app_info_->package_name) == 0 &&
      !task_id_.has_value()) {
      if (!app_launcher_) {
        VLOG(2) << "Starting kiosk app";
        observers_.NotifyAppPrepared();
        observers_.NotifyAppLaunching();
        app_launcher_ = std::make_unique<KioskArcvmAppLauncher>(
            ArcAppListPrefs::Get(profile_), app_id_, this);
        app_launcher_->LaunchApp(profile_);
      }
  } else if (task_id_.has_value()) {
    VLOG(2) << "Kiosk app already running";
  }
}

std::string KioskArcvmAppService::GetAppId() {
  // TODO(crbug.com/418871771): Refactor to ash::AnnotatedAccountId::Get
  AccountId account_id = multi_user_util::GetAccountIdFromProfile(profile_);
  const KioskArcvmAppData* app = app_manager_->GetAppByAccountId(account_id);
  if (!app) {
    return std::string();
  }
  std::unordered_set<std::string> app_ids =
      ArcAppListPrefs::Get(profile_)->GetAppsForPackage(app->package_name());
  if (app_ids.empty()) {
    return std::string();
  }
  // If `activity` and `intent` are not specified, return any app from the
  // package.
  if (app->activity().empty() && app->intent().empty()) {
    return *app_ids.begin();
  }
  // Check that the app is registered for given package.
  return app_ids.count(app->app_id()) ? app->app_id() : std::string();
}

void KioskArcvmAppService::ResetAppLauncher() {
  app_launcher_.reset();
  task_id_.reset();
}

// KioskArcvmAppService manages its own state by itself.
void KioskArcvmAppService::Initialize() {}
void KioskArcvmAppService::ContinueWithNetworkReady() {}
void KioskArcvmAppService::LaunchApp() {}

}  // namespace ash
