// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_revocation_request.h"

#include "base/task/sequenced_task_runner.h"
#include "base/time/default_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/notifications_permission_revocation_config.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/constants.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permissions_client.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace {
constexpr char kExcludedKey[] = "exempted";
constexpr char kPermissionName[] = "notifications";

struct OriginStatus {
  bool is_exempt_from_future_revocations = false;
  bool has_been_previously_revoked = false;
};

OriginStatus GetOriginStatus(Profile* profile, const GURL& origin) {
  const base::Value stored_value =
      permissions::PermissionsClient::Get()
          ->GetSettingsMap(profile)
          ->GetWebsiteSetting(
              origin, GURL(),
              ContentSettingsType::PERMISSION_AUTOREVOCATION_DATA);

  OriginStatus status;

  if (!stored_value.is_dict())
    return status;

  const base::Value::Dict* dict =
      stored_value.GetDict().FindDict(kPermissionName);
  if (!dict)
    return status;

  if (dict->FindBool(kExcludedKey).has_value()) {
    status.is_exempt_from_future_revocations =
        dict->FindBool(kExcludedKey).value();
  }
  if (dict->FindBool(permissions::kRevokedKey).has_value()) {
    status.has_been_previously_revoked =
        dict->FindBool(permissions::kRevokedKey).value();
  }

  return status;
}

void SetOriginStatus(Profile* profile,
                     const GURL& origin,
                     const OriginStatus& status) {
  base::Value::Dict dict;
  base::Value::Dict permission_dict;
  permission_dict.Set(kExcludedKey, status.is_exempt_from_future_revocations);
  permission_dict.Set(permissions::kRevokedKey,
                      status.has_been_previously_revoked);
  dict.Set(kPermissionName, std::move(permission_dict));

  permissions::PermissionsClient::Get()
      ->GetSettingsMap(profile)
      ->SetWebsiteSettingDefaultScope(
          origin, GURL(), ContentSettingsType::PERMISSION_AUTOREVOCATION_DATA,
          base::Value(std::move(dict)));
}

void RevokePermission(const GURL& origin, Profile* profile) {
  permissions::PermissionsClient::Get()
      ->GetSettingsMap(profile)
      ->SetContentSettingDefaultScope(origin, GURL(),
                                      ContentSettingsType::NOTIFICATIONS,
                                      ContentSetting::CONTENT_SETTING_DEFAULT);

  OriginStatus status = GetOriginStatus(profile, origin);
  status.has_been_previously_revoked = true;
  SetOriginStatus(profile, origin, status);

  permissions::PermissionUmaUtil::PermissionRevoked(
      ContentSettingsType::NOTIFICATIONS,
      permissions::PermissionSourceUI::AUTO_REVOCATION, origin, profile);
}
}  // namespace

PermissionRevocationRequest::PermissionRevocationRequest(
    Profile* profile,
    const GURL& origin,
    OutcomeCallback callback)
    : profile_(profile), origin_(origin), callback_(std::move(callback)) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PermissionRevocationRequest::CheckAndRevokeIfBlocklisted,
                     weak_factory_.GetWeakPtr()));
}

PermissionRevocationRequest::~PermissionRevocationRequest() = default;

// static
void PermissionRevocationRequest::ExemptOriginFromFutureRevocations(
    Profile* profile,
    const GURL& origin) {
  OriginStatus status = GetOriginStatus(profile, origin);
  status.is_exempt_from_future_revocations = true;
  SetOriginStatus(profile, origin, status);
}

// static
bool PermissionRevocationRequest::IsOriginExemptedFromFutureRevocations(
    Profile* profile,
    const GURL& origin) {
  OriginStatus status = GetOriginStatus(profile, origin);
  return status.is_exempt_from_future_revocations;
}

// static
bool PermissionRevocationRequest::HasPreviouslyRevokedPermission(
    Profile* profile,
    const GURL& origin) {
  OriginStatus status = GetOriginStatus(profile, origin);
  return status.has_been_previously_revoked;
}

void PermissionRevocationRequest::CheckAndRevokeIfBlocklisted() {
  DCHECK(profile_);
  DCHECK(callback_);

  if (!safe_browsing::IsSafeBrowsingEnabled(*profile_->GetPrefs()) ||
      IsOriginExemptedFromFutureRevocations(profile_, origin_) ||
      (!NotificationsPermissionRevocationConfig::
           IsAbusiveOriginPermissionRevocationEnabled() &&
       !NotificationsPermissionRevocationConfig::
           IsDisruptiveOriginPermissionRevocationEnabled())) {
    NotifyCallback(Outcome::PERMISSION_NOT_REVOKED);
    return;
  }

  CrowdDenyPreloadData* crowd_deny = CrowdDenyPreloadData::GetInstance();
  permissions::PermissionUmaUtil::RecordCrowdDenyVersionAtAbuseCheckTime(
      crowd_deny->version_on_disk());

  if (!crowd_deny->IsReadyToUse())
    crowd_deny_request_start_time_ = base::TimeTicks::Now();

  crowd_deny->GetReputationDataForSiteAsync(
      url::Origin::Create(origin_),
      base::BindOnce(&PermissionRevocationRequest::OnSiteReputationReady,
                     weak_factory_.GetWeakPtr()));
}

void PermissionRevocationRequest::OnSiteReputationReady(
    const CrowdDenyPreloadData::SiteReputation* site_reputation) {
  if (crowd_deny_request_start_time_.has_value()) {
    crowd_deny_request_duration_ =
        base::TimeTicks::Now() - crowd_deny_request_start_time_.value();
  }

  if (site_reputation && !site_reputation->warning_only()) {
    bool should_revoke_permission = false;
    switch (site_reputation->notification_ux_quality()) {
      case CrowdDenyPreloadData::SiteReputation::ABUSIVE_PROMPTS:
      case CrowdDenyPreloadData::SiteReputation::ABUSIVE_CONTENT:
        should_revoke_permission = NotificationsPermissionRevocationConfig::
            IsAbusiveOriginPermissionRevocationEnabled();
        break;
      case CrowdDenyPreloadData::SiteReputation::DISRUPTIVE_BEHAVIOR:
        should_revoke_permission = NotificationsPermissionRevocationConfig::
            IsDisruptiveOriginPermissionRevocationEnabled();
        break;
      default:
        should_revoke_permission = false;
    }
    DCHECK(g_browser_process->safe_browsing_service());
    if (should_revoke_permission &&
        g_browser_process->safe_browsing_service()) {
      safe_browsing_request_.emplace(
          g_browser_process->safe_browsing_service()->database_manager(),
          base::DefaultClock::GetInstance(), url::Origin::Create(origin_),
          base::BindOnce(
              &PermissionRevocationRequest::OnSafeBrowsingVerdictReceived,
              weak_factory_.GetWeakPtr(), site_reputation));
      return;
    }
  }
  NotifyCallback(Outcome::PERMISSION_NOT_REVOKED);
}

void PermissionRevocationRequest::OnSafeBrowsingVerdictReceived(
    const CrowdDenyPreloadData::SiteReputation* site_reputation,
    CrowdDenySafeBrowsingRequest::Verdict verdict) {
  DCHECK(safe_browsing_request_);
  DCHECK(profile_);
  DCHECK(callback_);

  if (verdict == CrowdDenySafeBrowsingRequest::Verdict::kUnacceptable) {
    RevokePermission(origin_, profile_);
    if (site_reputation->notification_ux_quality() ==
            CrowdDenyPreloadData::SiteReputation::ABUSIVE_PROMPTS ||
        site_reputation->notification_ux_quality() ==
            CrowdDenyPreloadData::SiteReputation::ABUSIVE_CONTENT) {
      NotifyCallback(Outcome::PERMISSION_REVOKED_DUE_TO_ABUSE);
    } else if (site_reputation->notification_ux_quality() ==
               CrowdDenyPreloadData::SiteReputation::DISRUPTIVE_BEHAVIOR) {
      NotifyCallback(Outcome::PERMISSION_REVOKED_DUE_TO_DISRUPTIVE_BEHAVIOR);
    }
  } else {
    NotifyCallback(Outcome::PERMISSION_NOT_REVOKED);
  }
}

void PermissionRevocationRequest::NotifyCallback(Outcome outcome) {
  if (outcome == Outcome::PERMISSION_NOT_REVOKED &&
      crowd_deny_request_duration_.has_value()) {
    permissions::PermissionUmaUtil::RecordCrowdDenyDelayedPushNotification(
        crowd_deny_request_duration_.value());
  }

  std::move(callback_).Run(outcome);
}
