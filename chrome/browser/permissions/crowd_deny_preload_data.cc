// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/crowd_deny_preload_data.h"

#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/permissions/permission_uma_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace {

using DomainToReputationMap = CrowdDenyPreloadData::DomainToReputationMap;

// Attempts to load the preload data from |proto_path|, parse it as a serialized
// chrome_browser_crowd_deny::PreloadData message, and index it by domain.
// Returns an empty map is anything goes wrong.
DomainToReputationMap LoadAndParseAndIndexPreloadDataFromDisk(
    const base::FilePath& proto_path) {
  std::string binary_proto;
  if (!base::ReadFileToString(proto_path, &binary_proto))
    return {};

  CrowdDenyPreloadData::PreloadData preload_data;
  if (!preload_data.ParseFromString(binary_proto))
    return {};

  std::vector<DomainToReputationMap::value_type> domain_reputation_pairs;
  domain_reputation_pairs.reserve(preload_data.site_reputations_size());
  for (const auto& site_reputation : preload_data.site_reputations()) {
    domain_reputation_pairs.emplace_back(site_reputation.domain(),
                                         site_reputation);
  }
  return DomainToReputationMap(std::move(domain_reputation_pairs));
}

PendingOrigin::PendingOrigin(
    url::Origin origin,
    base::OnceCallback<void(const chrome_browser_crowd_deny::SiteReputation*)>
        callback)
    : origin(std::move(origin)), callback(std::move(callback)) {}
PendingOrigin::~PendingOrigin() = default;

}  // namespace

CrowdDenyPreloadData::CrowdDenyPreloadData() {
  loading_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
}

CrowdDenyPreloadData::~CrowdDenyPreloadData() = default;

// static
CrowdDenyPreloadData* CrowdDenyPreloadData::GetInstance() {
  static base::NoDestructor<CrowdDenyPreloadData> instance;
  return instance.get();
}

void CrowdDenyPreloadData::GetReputationDataForSiteAsync(
    const url::Origin& origin,
    SiteReputationCallback callback) {
  if (is_ready_to_use_) {
    std::move(callback).Run(GetReputationDataForSite(origin));
  } else {
    origins_pending_verification_.emplace(origin, std::move(callback));
  }
}

void CrowdDenyPreloadData::LoadFromDisk(const base::FilePath& proto_path,
                                        const base::Version& version) {
  version_on_disk_ = version;
  is_ready_to_use_ = false;
  // On failure, LoadAndParseAndIndexPreloadDataFromDisk will return an empty
  // map. Replace the in-memory state with that regardless, so that the stale
  // old data will no longer be used.
  loading_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&LoadAndParseAndIndexPreloadDataFromDisk, proto_path),
      base::BindOnce(&CrowdDenyPreloadData::SetSiteReputations,
                     weak_factory_.GetWeakPtr()));
}

const CrowdDenyPreloadData::SiteReputation*
CrowdDenyPreloadData::GetReputationDataForSite(
    const url::Origin& origin) const {
  if (origin.scheme() != url::kHttpsScheme)
    return nullptr;

  const auto it_exact_match = domain_to_reputation_map_.find(origin.host());
  if (it_exact_match != domain_to_reputation_map_.end())
    return &it_exact_match->second;

  const std::string registerable_domain =
      net::registry_controlled_domains::GetDomainAndRegistry(
          origin, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  const auto it_domain_suffix_match =
      domain_to_reputation_map_.find(registerable_domain);
  if (it_domain_suffix_match != domain_to_reputation_map_.end() &&
      it_domain_suffix_match->second.include_subdomains()) {
    return &it_domain_suffix_match->second;
  }

  return nullptr;
}

void CrowdDenyPreloadData::SetSiteReputations(DomainToReputationMap map) {
  domain_to_reputation_map_ = std::move(map);
  is_ready_to_use_ = true;

  CheckOriginsPendingVerification();
}

void CrowdDenyPreloadData::CheckOriginsPendingVerification() {
  if (origins_pending_verification_.empty())
    return;

  GetReputationDataForSiteAsync(
      origins_pending_verification_.front().origin,
      std::move(origins_pending_verification_.front().callback));
  origins_pending_verification_.pop();
  // The |origins_pending_verification_| might not be empty, check the next
  // item.
  CheckOriginsPendingVerification();
}

CrowdDenyPreloadData::DomainToReputationMap
CrowdDenyPreloadData::TakeSiteReputations() {
  return std::move(domain_to_reputation_map_);
}

// ScopedCrowdDenyPreloadDataOverride -----------------------------------

namespace testing {

ScopedCrowdDenyPreloadDataOverride::ScopedCrowdDenyPreloadDataOverride() {
  old_map_ = CrowdDenyPreloadData::GetInstance()->TakeSiteReputations();
}

ScopedCrowdDenyPreloadDataOverride::~ScopedCrowdDenyPreloadDataOverride() {
  CrowdDenyPreloadData::GetInstance()->SetSiteReputations(std::move(old_map_));
}

void ScopedCrowdDenyPreloadDataOverride::SetOriginReputation(
    const url::Origin& origin,
    SiteReputation site_reputation) {
  auto* instance = CrowdDenyPreloadData::GetInstance();
  DomainToReputationMap testing_map = instance->TakeSiteReputations();
  testing_map[origin.host()] = std::move(site_reputation);
  instance->SetSiteReputations(std::move(testing_map));
}

void ScopedCrowdDenyPreloadDataOverride::ClearAllReputations() {
  CrowdDenyPreloadData::GetInstance()->TakeSiteReputations();
}

}  // namespace testing
