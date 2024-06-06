// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extended_updates/extended_updates_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/system/extended_updates/extended_updates_metrics.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/model/update_model.h"
#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/time/default_clock.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/extended_updates/extended_updates_notification.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/ownership/owner_settings_service.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"

namespace ash {

namespace {

ExtendedUpdatesController* instance_ = nullptr;

// Returns true if the EOL params satisfy opt-in eligibility.
bool CheckEolParams(const ExtendedUpdatesController::Params& params) {
  // Valid date range is between extended date and eol date.
  // Extended date is expected to be before eol date.
  // Also, not eligible if opt-in is not required.
  return !params.eol_passed && params.extended_date_passed &&
         params.opt_in_required;
}

// Returns true if the user could have apps but doesn't have any Android apps.
bool HasNoAndroidApps(content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    // Likely incognito profile, which is not applicable here.
    return false;
  }

  if (!arc::IsArcPlayStoreEnabledForProfile(profile)) {
    // Play store turned off.
    return true;
  }

  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
  auto& registry = proxy->AppRegistryCache();
  if (!registry.IsAppTypeInitialized(apps::AppType::kArc)) {
    // If ARC app type hasn't been initialized by now, there are no ARC apps.
    return true;
  }

  bool has_arc_app = false;
  registry.ForEachApp([&has_arc_app](const apps::AppUpdate& update) {
    if (!has_arc_app && update.AppType() == apps::AppType::kArc &&
        update.Readiness() == apps::Readiness::kReady) {
      has_arc_app = true;
    }
  });
  return !has_arc_app;
}

}  // namespace

ExtendedUpdatesController* ExtendedUpdatesController::Get() {
  if (!instance_) {
    instance_ = new ExtendedUpdatesController();
  }
  return instance_;
}

void ExtendedUpdatesController::ResetInstanceForTesting() {
  if (instance_) {
    delete instance_;
    instance_ = nullptr;
  }
}

void ExtendedUpdatesController::
    RecordEntryPointEventForSettingsSetUpButtonShown() {
  RecordExtendedUpdatesEntryPointEvent(
      ExtendedUpdatesEntryPointEvent::kSettingsSetUpButtonShown);
}

void ExtendedUpdatesController::
    RecordEntryPointEventForSettingsSetUpButtonClicked() {
  RecordExtendedUpdatesEntryPointEvent(
      ExtendedUpdatesEntryPointEvent::kSettingsSetUpButtonClicked);
}

base::CallbackListSubscription
ExtendedUpdatesController::SubscribeToDeviceSettingsChanges(
    base::RepeatingClosure callback) {
  if (CrosSettings::IsInitialized()) {
    return CrosSettings::Get()->AddSettingsObserver(
        kDeviceExtendedAutoUpdateEnabled, std::move(callback));
  }
  return base::CallbackListSubscription();
}

ExtendedUpdatesController::ExtendedUpdatesController()
    : clock_(base::DefaultClock::GetInstance()) {
  SubscribeToDeviceSettingsChanges();
}

ExtendedUpdatesController::~ExtendedUpdatesController() = default;

ExtendedUpdatesController* ExtendedUpdatesController::SetInstanceForTesting(
    ExtendedUpdatesController* controller) {
  auto* old_instance = instance_;
  instance_ = controller;
  return old_instance;
}

bool ExtendedUpdatesController::IsOptInEligible(
    content::BrowserContext* context,
    const Params& params) {
  if (!CheckEolParams(params)) {
    return false;
  }
  return IsOptInEligible(context);
}

bool ExtendedUpdatesController::IsOptInEligible(
    content::BrowserContext* context) {
  auto* owner_settings =
      OwnerSettingsServiceAshFactory::GetForBrowserContext(context);
  return HasOptInAbility(owner_settings);
}

bool ExtendedUpdatesController::IsOptedIn() {
  bool value;
  if (CrosSettings::Get()->GetBoolean(kDeviceExtendedAutoUpdateEnabled,
                                      &value)) {
    return value;
  }
  return false;
}

bool ExtendedUpdatesController::OptIn(content::BrowserContext* context) {
  auto* owner_settings =
      OwnerSettingsServiceAshFactory::GetForBrowserContext(context);
  if (!HasOptInAbility(owner_settings)) {
    return false;
  }

  return owner_settings->SetBoolean(kDeviceExtendedAutoUpdateEnabled, true);
}

void ExtendedUpdatesController::OnEolInfo(
    content::BrowserContext* context,
    const UpdateEngineClient::EolInfo& eol_info) {
  if (!context || eol_info.eol_date.is_null() ||
      eol_info.extended_date.is_null()) {
    return;
  }

  const base::Time now = clock_->Now();
  Params params{
      .eol_passed = eol_info.eol_date <= now,
      .extended_date_passed = eol_info.extended_date <= now,
      .opt_in_required = eol_info.extended_opt_in_required,
  };
  if (!CheckEolParams(params)) {
    return;
  }

  auto* owner_settings =
      OwnerSettingsServiceAshFactory::GetForBrowserContext(context);
  if (!owner_settings) {
    // In some sessions OwnerSettingsService may be completely uninitialized,
    // for example Guest Mode.
    LOG(WARNING) << "OwnerSettingsService is uninitialized for the profile."
                    " Will not notify about extended updates";
    return;
  }
  // This function is called upon login, so owner settings may not have finished
  // loading yet. Defer decision to show notification until then.
  owner_settings->IsOwnerAsync(base::IgnoreArgs<bool>(
      base::BindOnce(&ExtendedUpdatesController::OnOwnershipDetermined,
                     weak_factory_.GetWeakPtr(), context->GetWeakPtr())));
}

void ExtendedUpdatesController::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

void ExtendedUpdatesController::OnOwnershipDetermined(
    base::WeakPtr<content::BrowserContext> context) {
  if (!context || !IsOptInEligible(context.get())) {
    return;
  }

  if (auto* system_tray_model = Shell::Get()->system_tray_model()) {
    system_tray_model->SetShowExtendedUpdatesNotice(true);
    SubscribeToDeviceSettingsChanges();
  }

  if (ShouldShowNotification(context.get())) {
    ShowNotification(context.get());
  }
}

// TODO(b/333767804): Show notification again if extended updates date changed.
bool ExtendedUpdatesController::ShouldShowNotification(
    content::BrowserContext* context) {
  if (!IsOptInEligible(context) || !HasNoAndroidApps(context)) {
    return false;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  if (ExtendedUpdatesNotification::IsNotificationDismissed(profile)) {
    return false;
  }
  return true;
}

void ExtendedUpdatesController::ShowNotification(
    content::BrowserContext* context) {
  ExtendedUpdatesNotification::Show(Profile::FromBrowserContext(context));
}

void ExtendedUpdatesController::SubscribeToDeviceSettingsChanges() {
  if (!settings_change_subscription_) {
    settings_change_subscription_ = SubscribeToDeviceSettingsChanges(
        base::BindRepeating(&ExtendedUpdatesController::OnDeviceSettingsChanged,
                            weak_factory_.GetWeakPtr()));
  }
}

void ExtendedUpdatesController::OnDeviceSettingsChanged() {
  if (IsOptedIn()) {
    if (auto* system_tray_model = Shell::Get()->system_tray_model()) {
      system_tray_model->SetShowExtendedUpdatesNotice(false);
    }
  }
}

bool ExtendedUpdatesController::HasOptInAbility(
    ownership::OwnerSettingsService* owner_settings) {
  // Only owner user can opt in.
  // By extension, only unmanaged devices can opt in.
  if (!owner_settings || !owner_settings->IsOwner()) {
    return false;
  }

  // Check feature enablement after other checks to reduce noise due to how
  // finch experiment is recorded.
  if (!ash::features::IsExtendedUpdatesOptInFeatureEnabled()) {
    return false;
  }

  // Only eligible if not already opted in.
  return !IsOptedIn();
}

}  // namespace ash
