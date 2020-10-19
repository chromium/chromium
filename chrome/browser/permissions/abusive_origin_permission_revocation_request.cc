// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/abusive_origin_permission_revocation_request.h"

#include "base/time/default_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/abusive_origin_notifications_permission_revocation_config.h"
#include "chrome/browser/permissions/crowd_deny_preload_data.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_result.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permissions_client.h"
#include "components/safe_browsing/core/db/database_manager.h"

namespace {
constexpr char kExcludedKey[] = "exempted";
constexpr char kRevokedKey[] = "revoked";
constexpr char kPermissionName[] = "notifications";

struct OriginStatus {
  bool is_exempt_from_future_revocations = false;
  bool has_been_previously_revoked = false;
};

OriginStatus GetOriginStatus(Profile* profile, const GURL& origin) {
  std::unique_ptr<base::Value> stored_value =
      permissions::PermissionsClient::Get()
          ->GetSettingsMap(profile)
          ->GetWebsiteSetting(
              origin, GURL(),
              ContentSettingsType::PERMISSION_AUTOREVOCATION_DATA,
              std::string(), nullptr);

  OriginStatus status;

  if (!stored_value || !stored_value->is_dict())
    return status;

  base::Value* dict = stored_value->FindPath(kPermissionName);
  if (!dict)
    return status;

  if (dict->FindBoolPath(kExcludedKey).has_value()) {
    status.is_exempt_from_future_revocations =
        dict->FindBoolPath(kExcludedKey).value();
  }
  if (dict->FindBoolPath(kRevokedKey).has_value()) {
    status.has_been_previously_revoked =
        dict->FindBoolPath(kRevokedKey).value();
  }

  return status;
}

void SetOriginStatus(Profile* profile,
                     const GURL& origin,
                     const OriginStatus& status) {
  base::Value dict(base::Value::Type::DICTIONARY);
  base::Value permission_dict(base::Value::Type::DICTIONARY);
  permission_dict.SetKey(kExcludedKey,
                         base::Value(status.is_exempt_from_future_revocations));
  permission_dict.SetKey(kRevokedKey,
                         base::Value(status.has_been_previously_revoked));
  dict.SetKey(kPermissionName, std::move(permission_dict));

  permissions::PermissionsClient::Get()
      ->GetSettingsMap(profile)
      ->SetWebsiteSettingDefaultScope(
          origin, GURL(), ContentSettingsType::PERMISSION_AUTOREVOCATION_DATA,
          std::string(), base::WrapUnique(dict.DeepCopy()));
}

void RevokePermission(const GURL& origin, Profile* profile) {
  permissions::PermissionsClient::Get()
      ->GetSettingsMap(profile)
      ->SetContentSettingDefaultScope(
          origin, GURL(), ContentSettingsType::NOTIFICATIONS, std::string(),
          ContentSetting::CONTENT_SETTING_DEFAULT);

  OriginStatus status = GetOriginStatus(profile, origin);
  status.has_been_previously_revoked = true;
  SetOriginStatus(profile, origin, status);

  permissions::PermissionUmaUtil::PermissionRevoked(
      ContentSettingsType::NOTIFICATIONS,
      permissions::PermissionSourceUI::AUTO_REVOCATION, origin, profile);
}
}  // namespace

AbusiveOriginPermissionRevocationRequest::
    AbusiveOriginPermissionRevocationRequest(Profile* profile,
                                             const GURL& origin,
                                             OutcomeCallback callback)
    : profile_(profile), origin_(origin), callback_(std::move(callback)) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AbusiveOriginPermissionRevocationRequest::CheckAndRevokeIfAbusive,
          weak_factory_.GetWeakPtr()));
}

AbusiveOriginPermissionRevocationRequest::
    ~AbusiveOriginPermissionRevocationRequest() = default;

void AbusiveOriginPermissionRevocationRequest::CheckAndRevokeIfAbusive() {
  DCHECK(profile_);
  DCHECK(callback_);

  if (!AbusiveOriginNotificationsPermissionRevocationConfig::IsEnabled()) {
    std::move(callback_).Run(Outcome::PERMISSION_NOT_REVOKED);
    return;
  }

  if (IsOriginExemptedFromFutureRevocations(profile_, origin_)) {
    std::move(callback_).Run(Outcome::PERMISSION_NOT_REVOKED);
    return;
  }

  CrowdDenyPreloadData* crowd_deny = CrowdDenyPreloadData::GetInstance();
  permissions::PermissionUmaUtil::RecordCrowdDenyIsLoadedAtAbuseCheckTime(
      crowd_deny->is_loaded_from_disk());
  permissions::PermissionUmaUtil::RecordCrowdDenyVersionAtAbuseCheckTime(
      crowd_deny->version_on_disk());

  const CrowdDenyPreloadData::SiteReputation* site_reputation =
      crowd_deny->GetReputationDataForSite(url::Origin::Create(origin_));
  if (site_reputation &&
      (site_reputation->notification_ux_quality() ==
           CrowdDenyPreloadData::SiteReputation::ABUSIVE_PROMPTS ||
       site_reputation->notification_ux_quality() ==
           CrowdDenyPreloadData::SiteReputation::ABUSIVE_CONTENT)) {
    DCHECK(g_browser_process->safe_browsing_service());

    if (g_browser_process->safe_browsing_service()) {
      safe_browsing_request_.emplace(
          g_browser_process->safe_browsing_service()->database_manager(),
          base::DefaultClock::GetInstance(), url::Origin::Create(origin_),
          base::BindOnce(&AbusiveOriginPermissionRevocationRequest::
                             OnSafeBrowsingVerdictReceived,
                         weak_factory_.GetWeakPtr()));
      return;
    }
  }

  std::move(callback_).Run(Outcome::PERMISSION_NOT_REVOKED);
}

void AbusiveOriginPermissionRevocationRequest::OnSafeBrowsingVerdictReceived(
    CrowdDenySafeBrowsingRequest::Verdict verdict) {
  DCHECK(safe_browsing_request_);
  DCHECK(profile_);
  DCHECK(callback_);

  if (verdict == CrowdDenySafeBrowsingRequest::Verdict::kUnacceptable) {
    RevokePermission(origin_, profile_);
    std::move(callback_).Run(Outcome::PERMISSION_REVOKED_DUE_TO_ABUSE);
  } else {
    std::move(callback_).Run(Outcome::PERMISSION_NOT_REVOKED);
  }
}

// static
bool AbusiveOriginPermissionRevocationRequest::
    IsOriginExemptedFromFutureRevocations(Profile* profile,
                                          const GURL& origin) {
  OriginStatus status = GetOriginStatus(profile, origin);
  return status.is_exempt_from_future_revocations;
}

// static
bool AbusiveOriginPermissionRevocationRequest::HasPreviouslyRevokedPermission(
    Profile* profile,
    const GURL& origin) {
  OriginStatus status = GetOriginStatus(profile, origin);
  return status.has_been_previously_revoked;
}

// static
void AbusiveOriginPermissionRevocationRequest::
    ExemptOriginFromFutureRevocations(Profile* profile, const GURL& origin) {
  OriginStatus status = GetOriginStatus(profile, origin);
  status.is_exempt_from_future_revocations = true;
  SetOriginStatus(profile, origin, status);
}
