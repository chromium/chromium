// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/third_party_data_remover.h"

#include <set>
#include <utility>

#include "base/metrics/user_metrics.h"
#include "cc/base/features.h"
#include "chrome/browser/browsing_data/access_context_audit_service.h"
#include "chrome/browser/browsing_data/access_context_audit_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/same_site_data_remover.h"
#include "url/origin.h"

namespace {

void OnGetStorageAccessRecords(
    base::OnceClosure closure,
    content::BrowserContext* context,
    std::vector<AccessContextAuditDatabase::AccessRecord> records) {
  std::set<url::Origin> origins;
  for (const auto& record : records) {
    origins.insert(std::move(record.origin));
  }
  content::ClearSameSiteNoneCookiesAndStorageForOrigins(
      std::move(closure), context, std::move(origins));
}

}  // namespace

void ClearThirdPartyData(base::OnceClosure closure,
                         content::BrowserContext* context) {
  base::RecordAction(
      base::UserMetricsAction("ClearBrowsingData_SameSiteNoneData"));
  // TODO(crbug.com/987177) Consider falling back on clearing storage for all
  // domains with SameSite=None cookies when it's the first time the client
  // is calling this function.
  if (base::FeatureList::IsEnabled(
          features::kClientStorageAccessContextAuditing)) {
    AccessContextAuditServiceFactory::GetForProfile(
        Profile::FromBrowserContext(context))
        ->GetThirdPartyStorageAccessRecords(base::BindOnce(
            &OnGetStorageAccessRecords, std::move(closure), context));
    return;
  }
  // If access context auditing is not enabled, fall back on deleting
  // SameSite=None cookies and storage for domains with those cookies.
  content::ClearSameSiteNoneData(std::move(closure), context);
}
