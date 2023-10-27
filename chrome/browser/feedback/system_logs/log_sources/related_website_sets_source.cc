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

std::string GetSiteType(const net::SiteType type) {
  switch (type) {
    case net::SiteType::kPrimary:
      return kRelatedWebsiteSetSPrimarySitesField;
    case net::SiteType::kAssociated:
      return kRelatedWebsiteSetAssociatedSitesField;
    case net::SiteType::kService:
      return kRelatedWebsiteSetServiceSitesField;
  }
}

}  // namespace

RelatedWebsiteSetsSource::RelatedWebsiteSetsSource()
    : SystemLogsSource("RelatedWebsiteSets") {}

RelatedWebsiteSetsSource::~RelatedWebsiteSetsSource() = default;

void RelatedWebsiteSetsSource::Fetch(SysLogsSourceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(callback);

  auto response = std::make_unique<SystemLogsResponse>();
  PopulateRelatedWebsiteSetsInfo(response.get());

  std::move(callback).Run(std::move(response));
}

// Populate the Related Website Sets info in the follow format:
// [ {
//   "AssociatedSites": [ "https://a.com", "https://b.com", "https://b.edu" ],
//   "PrimarySites": [ "https://example.com" ],
//   "ServiceSites": [ "https://c.com" ],
// }, {
//   "AssociatedSites": [ "https://a2.com", "https://b2.com" ],
//   "PrimarySites": [ "https://example2.com", "https://example2.com.co" ]
// } ]
void RelatedWebsiteSetsSource::PopulateRelatedWebsiteSetsInfo(
    SystemLogsResponse* response) {
  CHECK(response);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  Profile* profile = ProfileManager::GetActiveUserProfile();
#else
  Profile* profile = ProfileManager::GetLastUsedProfile();
#endif
  first_party_sets::FirstPartySetsPolicyService* service = first_party_sets::
      FirstPartySetsPolicyServiceFactory::GetForBrowserContext(profile);
  CHECK(service);

  if (!service->is_enabled()) {
    response->emplace(kSetsInfoField, kRelatedWebsiteSetsDisabled);
    return;
  }
  if (!service->is_ready()) {
    response->emplace(kSetsInfoField, "");
    return;
  }
  if (rws_info_.has_value()) {
    response->emplace(kSetsInfoField, rws_info_.value());
    return;
  }

  // Iterate through all RWS effective entries and form a dict of primary -> a
  // single set. A single set contains fields of "Primary", "AssociatedSites"
  // and "ServiceSites".
  base::Value::Dict rws_dict;
  service->ForEachEffectiveSetEntry([&](const net::SchemefulSite& site,
                                        const net::FirstPartySetEntry& entry) {
    const std::string site_str = site.Serialize();
    const std::string primary_site_str = entry.primary().Serialize();
    const std::string site_type_str = GetSiteType(entry.site_type());

    // Create a single set if one does not already exist for the primary.
    if (!rws_dict.contains(primary_site_str)) {
      rws_dict.Set(primary_site_str, base::Value::Dict());
    }
    base::Value* maybe_single_set = rws_dict.Find(primary_site_str);
    CHECK(maybe_single_set->is_dict());

    // Create a dict for each site type if one does not already exist.
    if (!maybe_single_set->GetDict().contains(site_type_str)) {
      maybe_single_set->GetDict().Set(site_type_str, base::Value::List());
    }
    base::Value* maybe_subset = maybe_single_set->GetDict().Find(site_type_str);
    CHECK(maybe_subset->is_list());
    maybe_subset->GetList().Append(site_str);
    return true;
  });

  // Iterate through the dict of primary -> a single set and put all the single
  // sets into a List.
  base::Value::List rws_list;
  for (auto [_, value] : rws_dict) {
    rws_list.Append(std::move(value));
  }

  std::string rws_info_str;
  base::JSONWriter::WriteWithOptions(
      rws_list, base::JsonOptions::OPTIONS_PRETTY_PRINT, &rws_info_str);
  rws_info_ = rws_info_str;
  response->emplace(kSetsInfoField, rws_info_str);
}

}  // namespace system_logs
