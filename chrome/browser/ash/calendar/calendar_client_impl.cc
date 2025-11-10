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
#include "chrome/browser/ash/calendar/calendar_keyed_service.h"
#include "components/policy/core/browser/url_list/policy_blocklist_service.h"
#include "components/policy/core/browser/url_list/url_blocklist_manager.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "google_apis/common/api_error_codes.h"
#include "url/gurl.h"

namespace ash {
namespace {

constexpr char kCalendarUrl[] = "https://calendar.google.com/";

}  // namespace

CalendarClientImpl::CalendarClientImpl(
    PrefService* pref_service,
    apps::AppServiceProxy* app_service_proxy,
    PolicyBlocklistService* policy_blocklist_service,
    CalendarKeyedService* calendar_keyed_service)
    : pref_service_(pref_service),
      app_service_proxy_(app_service_proxy),
      policy_blocklist_service_(policy_blocklist_service),
      calendar_keyed_service_(calendar_keyed_service) {}

CalendarClientImpl::~CalendarClientImpl() = default;

bool CalendarClientImpl::IsDisabledByAdmin() const {
  // 1) Check the Calendar pref.
  if (!pref_service_ ||
      !pref_service_->GetBoolean(prefs::kCalendarIntegrationEnabled) ||
      !base::Contains(pref_service_->GetList(
                          prefs::kContextualGoogleIntegrationsConfiguration),
                      prefs::kGoogleCalendarIntegrationName)) {
    RecordContextualGoogleIntegrationStatus(
        prefs::kGoogleCalendarIntegrationName,
        ContextualGoogleIntegrationStatus::kDisabledByPolicy);
    return true;
  }

  // 2) Check if the Calendar app is disabled by policy.
  if (!app_service_proxy_) {
    return true;
  }
  auto calendar_app_readiness = apps::Readiness::kUnknown;
  app_service_proxy_->AppRegistryCache().ForOneApp(
      ash::kGoogleCalendarAppId,
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
  if (!policy_blocklist_service_ ||
      policy_blocklist_service_->GetURLBlocklistState(GURL(kCalendarUrl)) ==
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

  // For non-gaia users this `calendar_keyed_service_` is not set.
  if (calendar_keyed_service_) {
    return calendar_keyed_service_->GetCalendarList(std::move(callback));
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

  // For non-gaia users this `calendar_keyed_service_` is not set.
  if (calendar_keyed_service_) {
    return calendar_keyed_service_->GetEventList(std::move(callback),
                                                 start_time, end_time);
  }

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

  // For non-gaia users this `calendar_keyed_service_` is not set.
  if (calendar_keyed_service_) {
    return calendar_keyed_service_->GetEventList(
        std::move(callback), start_time, end_time, calendar_id,
        calendar_color_id);
  }

  std::move(callback).Run(google_apis::OTHER_ERROR, /*events=*/nullptr);

  return base::DoNothing();
}

}  // namespace ash
