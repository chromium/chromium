// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/calendar/calendar_client_impl.h"

#include <string>
#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/web_app_id_constants.h"
#include "ash/glanceables/glanceables_metrics.h"
#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/calendar/calendar_keyed_service.h"
#include "chrome/browser/ash/calendar/calendar_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/content/policy_blocklist_service.h"
#include "components/policy/core/browser/url_blocklist_manager.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "google_apis/common/api_error_codes.h"
#include "url/gurl.h"

namespace ash {
namespace {

constexpr char kCalendarUrl[] = "https://calendar.google.com/";

}  // namespace

CalendarClientImpl::CalendarClientImpl(Profile* profile) : profile_(profile) {}

CalendarClientImpl::~CalendarClientImpl() = default;

bool CalendarClientImpl::IsDisabledByAdmin() const {
  // 1) Check the Calendar pref.
  const auto* const pref_service = profile_->GetPrefs();
  if (!pref_service ||
      !pref_service->GetBoolean(prefs::kCalendarIntegrationEnabled) ||
      !base::Contains(pref_service->GetList(
                          prefs::kContextualGoogleIntegrationsConfiguration),
                      prefs::kGoogleCalendarIntegrationName)) {
    RecordContextualGoogleIntegrationStatus(
        prefs::kGoogleCalendarIntegrationName,
        ContextualGoogleIntegrationStatus::kDisabledByPolicy);
    return true;
  }

  // 2) Check if the Calendar app is disabled by policy.
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
          profile_)) {
    return true;
  }
  auto calendar_app_readiness = apps::Readiness::kUnknown;
  apps::AppServiceProxyFactory::GetForProfile(profile_)
      ->AppRegistryCache()
      .ForOneApp(web_app::kGoogleCalendarAppId,
                 [&calendar_app_readiness](const apps::AppUpdate& update) {
                   calendar_app_readiness = update.Readiness();
                 });
  if (calendar_app_readiness == apps::Readiness::kDisabledByPolicy) {
    RecordContextualGoogleIntegrationStatus(
        prefs::kGoogleCalendarIntegrationName,
        ContextualGoogleIntegrationStatus::kDisabledByAppBlock);
    return true;
  }

  // 3) Check if the Calendar URL is blocked by policy.
  const auto* const policy_blocklist_service =
      PolicyBlocklistFactory::GetForBrowserContext(profile_);
  if (!policy_blocklist_service ||
      policy_blocklist_service->GetURLBlocklistState(GURL(kCalendarUrl)) ==
          policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST) {
    RecordContextualGoogleIntegrationStatus(
        prefs::kGoogleCalendarIntegrationName,
        ContextualGoogleIntegrationStatus::kDisabledByUrlBlock);
    return true;
  }

  RecordContextualGoogleIntegrationStatus(
      prefs::kGoogleCalendarIntegrationName,
      ContextualGoogleIntegrationStatus::kEnabled);
  return false;
}

base::OnceClosure CalendarClientImpl::GetCalendarList(
    google_apis::calendar::CalendarListCallback callback) {
  if (IsDisabledByAdmin()) {
    std::move(callback).Run(google_apis::OTHER_ERROR, /*calendars=*/nullptr);
    return base::DoNothing();
  }

  CalendarKeyedService* service =
      CalendarKeyedServiceFactory::GetInstance()->GetService(profile_);

  // For non-gaia users this `service` is not set.
  if (service) {
    return service->GetCalendarList(std::move(callback));
  }

  std::move(callback).Run(google_apis::OTHER_ERROR, /*calendars=*/nullptr);

  return base::DoNothing();
}

base::OnceClosure CalendarClientImpl::GetEventList(
    google_apis::calendar::CalendarEventListCallback callback,
    const base::Time start_time,
    const base::Time end_time) {
  if (IsDisabledByAdmin()) {
    std::move(callback).Run(google_apis::OTHER_ERROR, /*events=*/nullptr);
    return base::DoNothing();
  }

  CalendarKeyedService* service =
      CalendarKeyedServiceFactory::GetInstance()->GetService(profile_);

  // For non-gaia users this `service` is not set.
  if (service)
    return service->GetEventList(std::move(callback), start_time, end_time);

  std::move(callback).Run(google_apis::OTHER_ERROR, /*events=*/nullptr);

  return base::DoNothing();
}

base::OnceClosure CalendarClientImpl::GetEventList(
    google_apis::calendar::CalendarEventListCallback callback,
    const base::Time start_time,
    const base::Time end_time,
    const std::string& calendar_id,
    const std::string& calendar_color_id) {
  if (IsDisabledByAdmin()) {
    std::move(callback).Run(google_apis::OTHER_ERROR, /*events=*/nullptr);
    return base::DoNothing();
  }

  CalendarKeyedService* service =
      CalendarKeyedServiceFactory::GetInstance()->GetService(profile_);

  // For non-gaia users this `service` is not set.
  if (service) {
    return service->GetEventList(std::move(callback), start_time, end_time,
                                 calendar_id, calendar_color_id);
  }

  std::move(callback).Run(google_apis::OTHER_ERROR, /*events=*/nullptr);

  return base::DoNothing();
}

}  // namespace ash
