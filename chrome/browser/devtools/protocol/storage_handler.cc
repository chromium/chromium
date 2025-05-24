// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/storage_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/protocol/protocol.h"
#include "chrome/browser/devtools/protocol/storage.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/tpcd/metadata/manager_factory.h"
#include "content/public/browser/btm_service.h"
#include "content/public/browser/web_contents.h"
#include "net/first_party_sets/first_party_set_entry.h"

StorageHandler::StorageHandler(content::WebContents* web_contents,
                               protocol::UberDispatcher* dispatcher)
    : web_contents_(web_contents->GetWeakPtr()) {
  protocol::Storage::Dispatcher::wire(dispatcher, this);
}

StorageHandler::~StorageHandler() = default;

// TODO: crbug.com/380896828 - move CDP support for BTM to //content.
void StorageHandler::RunBounceTrackingMitigations(
    std::unique_ptr<RunBounceTrackingMitigationsCallback> callback) {
  content::BtmService* btm_service =
      web_contents_
          ? content::BtmService::Get(web_contents_->GetBrowserContext())
          : nullptr;

  if (!btm_service) {
    callback->sendFailure(protocol::Response::ServerError("No BtmService"));
    return;
  }

  btm_service->DeleteEligibleSitesImmediately(
      base::BindOnce(&StorageHandler::GotDeletedSites, std::move(callback)));
}

void StorageHandler::GetRelatedWebsiteSets(
    std::unique_ptr<GetRelatedWebsiteSetsCallback> callback) {
  first_party_sets::FirstPartySetsPolicyService* rws_service =
      web_contents_
          ? first_party_sets::FirstPartySetsPolicyServiceFactory::
                GetForBrowserContext(web_contents_->GetBrowserContext())
          : nullptr;

  if (!rws_service) {
    callback->sendFailure(
        protocol::Response::ServerError("No RelatedWebsiteSets Service"));
    return;
  }

  auto protocol_list =
      std::make_unique<protocol::Array<protocol::Storage::RelatedWebsiteSet>>();

  std::map<std::string, std::map<net::SiteType, std::set<std::string>>> sets;
  bool success = rws_service->ForEachEffectiveSetEntry(
      [&](const net::SchemefulSite& site,
          const net::FirstPartySetEntry& entry) {
        sets[entry.primary().Serialize()][entry.site_type()].insert(
            site.Serialize());
        return true;
      });
  if (!success) {
    callback->sendFailure(
        protocol::Response::ServerError("Failed fetching RelatedWebsiteSets"));
    return;
  }

  protocol_list->reserve(sets.size());
  for (auto& [unused_primary, set] : sets) {
    auto primary_sites = std::make_unique<protocol::Array<protocol::String>>();
    auto associated_sites =
        std::make_unique<protocol::Array<protocol::String>>();
    auto service_sites = std::make_unique<protocol::Array<protocol::String>>();
    for (auto& [site_type, sites] : set) {
      switch (site_type) {
        case net::SiteType::kPrimary:
          for (auto& site : sites) {
            primary_sites->push_back(std::move(site));
          }
          break;
        case net::SiteType::kAssociated:
          for (auto& site : sites) {
            associated_sites->push_back(std::move(site));
          }
          break;
        case net::SiteType::kService:
          for (auto& site : sites) {
            service_sites->push_back(std::move(site));
          }
          break;
      }
    }
    protocol_list->push_back(
        protocol::Storage::RelatedWebsiteSet::Create()
            .SetPrimarySites(std::move(primary_sites))
            .SetAssociatedSites(std::move(associated_sites))
            .SetServiceSites(std::move(service_sites))
            .Build());
  }
  callback->sendSuccess(std::move(protocol_list));
}

protocol::Response StorageHandler::GetAffectedUrlsForThirdPartyCookieMetadata(
    const protocol::String& first_party_url,
    std::unique_ptr<protocol::Array<protocol::String>> third_party_urls,
    std::unique_ptr<protocol::Array<protocol::String>>* matched_urls) {
  // The frontend should ensure that each call at least passes the current page
  // URL.
  const GURL first_party_gurl(first_party_url);
  if (!first_party_gurl.is_valid()) {
    return protocol::Response::InvalidParams(
        "Invalid first-party URL provided.");
  }

  tpcd::metadata::Manager* tpcd_metadata_manager =
      tpcd::metadata::ManagerFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
  if (!tpcd_metadata_manager) {
    return protocol::Response::InternalError();
  }

  *matched_urls = std::make_unique<protocol::Array<protocol::String>>();

  // If the first-party URL has a first-party pattern without a third-party
  // pattern defined, all URLs will be matched. Instead just return the first-
  // party URL.
  if (tpcd_metadata_manager->IsAllowed(GURL(), first_party_gurl, nullptr)) {
    (*matched_urls)->push_back(std::move(first_party_url));
    return protocol::Response::Success();
  }

  CHECK(third_party_urls);
  for (const auto& url : *third_party_urls) {
    const GURL gurl(url);
    if (!gurl.is_valid()) {
      return protocol::Response::InvalidParams(
          "Invalid third-party URL provided.");
    }

    if (tpcd_metadata_manager->IsAllowed(gurl, first_party_gurl, nullptr)) {
      (*matched_urls)->push_back(std::move(url));
    }
  }

  return protocol::Response::Success();
}

/* static */
void StorageHandler::GotDeletedSites(
    std::unique_ptr<RunBounceTrackingMitigationsCallback> callback,
    const std::vector<std::string>& sites) {
  auto deleted_sites =
      std::make_unique<protocol::Array<protocol::String>>(sites);
  callback->sendSuccess(std::move(deleted_sites));
}
