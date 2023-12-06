// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/system_logs/log_sources/related_website_sets_source.h"

#include <set>
#include <utility>

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"
#include "components/feedback/system_logs/system_logs_source.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"

namespace system_logs {

namespace {

constexpr char kRelatedWebsiteSetsDisabled[] = "Disabled";

constexpr char kRelatedWebsiteSetSPrimarySitesField[] = "PrimarySites";
constexpr char kRelatedWebsiteSetAssociatedSitesField[] = "AssociatedSites";
constexpr char kRelatedWebsiteSetServiceSitesField[] = "ServiceSites";
constexpr char kSerializationError[] = "Serialization Error";

const char* GetSiteType(const net::SiteType type) {
  switch (type) {
    case net::SiteType::kPrimary:
      return kRelatedWebsiteSetSPrimarySitesField;
    case net::SiteType::kAssociated:
      return kRelatedWebsiteSetAssociatedSitesField;
    case net::SiteType::kService:
      return kRelatedWebsiteSetServiceSitesField;
  }
}

// Computes the Related Website Sets info in the following format:
// [ {
//   "AssociatedSites": [ "https://a.com", "https://b.com", "https://b.edu" ],
//   "PrimarySites": [ "https://example.com" ],
//   "ServiceSites": [ "https://c.com" ],
// }, {
//   "AssociatedSites": [ "https://a2.com", "https://b2.com" ],
//   "PrimarySites": [ "https://example2.com", "https://example2.com.co" ]
// } ]
std::string ComputeRelatedWebsiteSetsInfo(
    base::WeakPtr<first_party_sets::FirstPartySetsPolicyService> service) {
  if (!service || !service->is_enabled()) {
    return kRelatedWebsiteSetsDisabled;
  }
  if (!service->is_ready()) {
    return "";
  }

  // Collect all effective RWS entries for this profile.
  std::map<std::string, std::map<net::SiteType, std::set<std::string>>> sets;
  service->ForEachEffectiveSetEntry([&](const net::SchemefulSite& site,
                                        const net::FirstPartySetEntry& entry) {
    sets[entry.primary().Serialize()][entry.site_type()].insert(
        site.Serialize());
    return true;
  });

  base::Value::List list;
  list.reserve(sets.size());
  for (auto& [unused_primary, set] : sets) {
    base::Value::Dict value;
    for (auto& [site_type, sites] : set) {
      base::Value::List* subset = value.EnsureList(GetSiteType(site_type));
      subset->reserve(sites.size());
      for (auto& site : sites) {
        subset->Append(std::move(site));
      }
    }
    list.Append(std::move(value));
  }

  return base::WriteJsonWithOptions(list,
                                    base::JsonOptions::OPTIONS_PRETTY_PRINT)
      .value_or(kSerializationError);
}

}  // namespace

RelatedWebsiteSetsSource::RelatedWebsiteSetsSource(
    first_party_sets::FirstPartySetsPolicyService* service)
    : SystemLogsSource("RelatedWebsiteSets"), service_(service->GetWeakPtr()) {}

RelatedWebsiteSetsSource::~RelatedWebsiteSetsSource() = default;

void RelatedWebsiteSetsSource::Fetch(SysLogsSourceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(callback);

  auto response = std::make_unique<SystemLogsResponse>();
  response->emplace(kSetsInfoField, ComputeRelatedWebsiteSetsInfo(service_));

  std::move(callback).Run(std::move(response));
}

}  // namespace system_logs
