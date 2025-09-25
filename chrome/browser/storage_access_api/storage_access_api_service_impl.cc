// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_access_api/storage_access_api_service_impl.h"

#include <utility>

#include "base/check.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace {
constexpr base::TimeDelta kTimerPeriod = base::Days(1);
}

StorageAccessAPIServiceImpl::StorageAccessAPIServiceImpl(
    content::BrowserContext* browser_context)
    : browser_context_(
          raw_ref<content::BrowserContext>::from_ptr(browser_context)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  periodic_timer_.Start(
      FROM_HERE, kTimerPeriod,
      base::BindRepeating(&StorageAccessAPIServiceImpl::OnPeriodicTimerFired,
                          weak_ptr_factory_.GetWeakPtr()));

  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile) {
    return;
  }
  PrefService* prefs = profile->GetPrefs();
  if (!prefs) {
    return;
  }
  // None of the parameters matter here except the type and the name; they must
  // match the usage at
  // https://crsrc.org/c/components/content_settings/core/browser/content_settings_registry.cc;drc=e8fbdf97bea0f7e89f2b12f4127bb651878b8660;l=348.
  content_settings::WebsiteSettingsInfo info(
      ContentSettingsType::STORAGE_ACCESS_HEADER_ORIGIN_TRIAL,
      "storage-access-header-origin-trial", base::Value(CONTENT_SETTING_BLOCK),
      content_settings::WebsiteSettingsInfo::UNSYNCABLE,
      content_settings::WebsiteSettingsInfo::NOT_LOSSY,
      content_settings::WebsiteSettingsInfo::
          REQUESTING_ORIGIN_AND_TOP_SCHEMEFUL_SITE_SCOPE,
      content_settings::WebsiteSettingsInfo::INHERIT_IN_INCOGNITO);

  prefs->ClearPref(info.pref_name());
  prefs->ClearPref(info.default_value_pref_name());
}

StorageAccessAPIServiceImpl::~StorageAccessAPIServiceImpl() = default;

std::optional<base::TimeDelta>
StorageAccessAPIServiceImpl::RenewPermissionGrant(
    const url::Origin& embedded_origin,
    const url::Origin& top_frame_origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!embedded_origin.opaque());
  CHECK(!top_frame_origin.opaque());

  if (embedded_origin.scheme() != url::kHttpsScheme ||
      top_frame_origin.scheme() != url::kHttpsScheme ||
      !updated_grants_.Insert(embedded_origin, top_frame_origin)) {
    return std::nullopt;
  }

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(&*browser_context_);
  CHECK(settings_map);

  return settings_map->RenewContentSetting(
      embedded_origin.GetURL(), top_frame_origin.GetURL(),
      ContentSettingsType::STORAGE_ACCESS, CONTENT_SETTING_ALLOW);
}

void StorageAccessAPIServiceImpl::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void StorageAccessAPIServiceImpl::OnPeriodicTimerFired() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  updated_grants_.Clear();
}

bool StorageAccessAPIServiceImpl::IsTimerRunningForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return periodic_timer_.IsRunning();
}
