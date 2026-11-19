// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STORAGE_ACCESS_API_STORAGE_ACCESS_METRICS_H_
#define CHROME_BROWSER_STORAGE_ACCESS_API_STORAGE_ACCESS_METRICS_H_

#include "chrome/browser/storage_access_api/storage_access_grant_permission_context.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace net {
class SchemefulSite;
}  // namespace net

// Not built when `use_fuzzing_engine` is set; see BUILD.gn.

namespace storage_access_api {

// Reports `outcome` for `requesting_site` to Private Insights. No-op unless
// `private_insights::kPrivateInsightsStorageAccess` is enabled,
// `browser_context` has a PrivateInsightsService (it has none in Incognito,
// Guest or system profiles), and the site is reportable.
//
// A site is reportable iff its host has a known public registry; IP addresses,
// single-label hosts such as "localhost" or "intranet", unknown TLDs such as
// "corp", hosts that are themselves a public suffix, and opaque or non-HTTP
// sites are dropped. The reported domain is the eTLD+1 computed against the
// ICANN section of the Public Suffix List only, so that e.g. "alice.github.io"
// is reported as "github.io".
//
// The service additionally drops the event without UMA and UKM consent; see
// `PrivateInsightsService::SetStorageAccessPermissionCallback()`.
void RecordOutcomeToPrivateInsights(content::BrowserContext* browser_context,
                                    RequestOutcome outcome,
                                    const net::SchemefulSite& requesting_site);

}  // namespace storage_access_api

#endif  // CHROME_BROWSER_STORAGE_ACCESS_API_STORAGE_ACCESS_METRICS_H_
