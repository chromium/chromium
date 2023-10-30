// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/system_logs/log_sources/related_website_sets_source.h"

#include <utility>

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
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
std::string ComputeRelatedWebsiteSetsInfo() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  Profile* profile = ProfileManager::GetActiveUserProfile();
#else
  Profile* profile = ProfileManager::GetLastUsedProfile();
#endif
  first_party_sets::FirstPartySetsPolicyService* service = first_party_sets::
      FirstPartySetsPolicyServiceFactory::GetForBrowserContext(profile);
  CHECK(service);

  if (!service->is_enabled()) {
    return kRelatedWebsiteSetsDisabled;
  }
  if (!service->is_ready()) {
    return "";
  }

  // Iterate through all RWS effective entries and form a dict of primary -> a
  // single set. A single set contains fields of "PrimarySites",
  // "AssociatedSites" and "ServiceSites".
  std::map<std::string, base::Value::Dict> rws_dict;
  service->ForEachEffectiveSetEntry([&](const net::SchemefulSite& site,
                                        const net::FirstPartySetEntry& entry) {
    rws_dict[entry.primary().Serialize()]
        .EnsureList(GetSiteType(entry.site_type()))
        ->Append(site.Serialize());
    return true;
  });

  base::Value::List rws_list;
  for (auto& [_, value] : rws_dict) {
    rws_list.Append(std::move(value));
  }

  return base::WriteJsonWithOptions(rws_list,
                                    base::JsonOptions::OPTIONS_PRETTY_PRINT)
      .value_or(kSerializationError);
}

}  // namespace

RelatedWebsiteSetsSource::RelatedWebsiteSetsSource()
    : SystemLogsSource("RelatedWebsiteSets") {}

RelatedWebsiteSetsSource::~RelatedWebsiteSetsSource() = default;

void RelatedWebsiteSetsSource::Fetch(SysLogsSourceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(callback);

  auto response = std::make_unique<SystemLogsResponse>();
  response->emplace(kSetsInfoField, ComputeRelatedWebsiteSetsInfo());

  std::move(callback).Run(std::move(response));
}

}  // namespace system_logs
