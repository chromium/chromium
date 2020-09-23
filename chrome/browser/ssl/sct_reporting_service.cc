// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/sct_reporting_service.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"

SCTReportingService::SCTReportingService(
    safe_browsing::SafeBrowsingService* safe_browsing_service,
    Profile* profile)
    : safe_browsing_service_(safe_browsing_service),
      pref_service_(*profile->GetPrefs()),
      profile_(profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Subscribe to SafeBrowsing preference change notifications. The initial Safe
  // Browsing state gets emitted to subscribers during Profile creation.
  safe_browsing_state_subscription_ =
      safe_browsing_service_->RegisterStateCallback(base::BindRepeating(
          &SCTReportingService::OnPreferenceChanged, base::Unretained(this)));
}

SCTReportingService::~SCTReportingService() = default;

namespace {
void SetSCTAuditingEnabledForStoragePartition(
    bool enabled,
    content::StoragePartition* storage_partition) {
  storage_partition->GetNetworkContext()->SetSCTAuditingEnabled(enabled);
}
}  // namespace

void SCTReportingService::SetReportingEnabled(bool enabled) {
  // Iterate over StoragePartitions for this Profile, and for each get the
  // NetworkContext and enable or disable SCT auditing.
  content::BrowserContext::ForEachStoragePartition(
      profile_,
      base::BindRepeating(&SetSCTAuditingEnabledForStoragePartition, enabled));

  if (!enabled)
    content::GetNetworkService()->ClearSCTAuditingCache();
}

void SCTReportingService::OnPreferenceChanged() {
  const bool enabled = safe_browsing_service_ &&
                       safe_browsing_service_->enabled_by_prefs() &&
                       safe_browsing::IsExtendedReportingEnabled(pref_service_);
  SetReportingEnabled(enabled);
}

void SCTReportingService::OnSCTReportReady(const std::string& cache_key) {
  for (TestObserver& observer : test_observers_)
    observer.OnSCTReportReady(cache_key);
  // TODO(crbug.com/1082860): Send the report using the Safe Browsing network
  // context.
}

void SCTReportingService::AddObserverForTesting(TestObserver* observer) {
  test_observers_.AddObserver(observer);
}

void SCTReportingService::RemoveObserverForTesting(TestObserver* observer) {
  test_observers_.RemoveObserver(observer);
}
