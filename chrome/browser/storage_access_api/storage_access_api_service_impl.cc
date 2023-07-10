// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/storage_access_api/storage_access_api_service_impl.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

constexpr base::TimeDelta kTimerPeriod = base::Days(1);

StorageAccessAPIServiceImpl::StorageAccessAPIServiceImpl(
    content::BrowserContext* browser_context)
    : browser_context_(
          raw_ref<content::BrowserContext>::from_ptr(browser_context)),
      grant_refreshes_enabled_(
          blink::features::kStorageAccessAPIRefreshGrantsOnUserInteraction
              .Get()) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!grant_refreshes_enabled_) {
    return;
  }

  periodic_timer_.Start(
      FROM_HERE, kTimerPeriod,
      base::BindRepeating(&StorageAccessAPIServiceImpl::OnPeriodicTimerFired,
                          weak_ptr_factory_.GetWeakPtr()));
}

StorageAccessAPIServiceImpl::~StorageAccessAPIServiceImpl() = default;

bool StorageAccessAPIServiceImpl::RenewPermissionGrant(
    const url::Origin& embedded_origin,
    const url::Origin& top_frame_origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!embedded_origin.opaque());
  CHECK(!top_frame_origin.opaque());

  if (!grant_refreshes_enabled_ ||
      embedded_origin.scheme() != url::kHttpsScheme ||
      top_frame_origin.scheme() != url::kHttpsScheme ||
      !updated_grants_.Insert(embedded_origin, top_frame_origin)) {
    return false;
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
  CHECK(grant_refreshes_enabled_);
  updated_grants_.Clear();
}

bool StorageAccessAPIServiceImpl::IsTimerRunningForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return periodic_timer_.IsRunning();
}
