// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/smb_client/smb_share_finder.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ash/smb_client/discovery/mdns_host_locator.h"
#include "chrome/browser/ash/smb_client/smb_constants.h"
#include "chrome/browser/ash/smb_client/smb_errors.h"

namespace ash::smb_client {

SmbShareFinder::SmbShareFinder(SmbProviderClient* client) : client_(client) {}
SmbShareFinder::~SmbShareFinder() = default;

void SmbShareFinder::GatherSharesInNetwork(
    HostDiscoveryResponse discovery_callback,
    GatherSharesInNetworkResponse shares_callback) {
  const bool is_host_discovery_pending = !discovery_callbacks_.empty();
  const bool is_share_discovery_pending = !share_callbacks_.empty();

  if (is_host_discovery_pending) {
    // Host discovery is currently running, add both |discovery_callback| and
    // |share_callback| to their respective vectors.
    InsertDiscoveryAndShareCallbacks(std::move(discovery_callback),
                                     std::move(shares_callback));
    return;
  }

  if (is_share_discovery_pending) {
    // Host discovery is complete but there are still share callbacks pending.
    // Run |discovery_callback| because pending share discoveries and no pending
    // host discoveries indicate that a host discovery must have recently
    // completed.
    std::move(discovery_callback).Run();
    InsertShareCallback(std::move(shares_callback));
    return;
  }

  // No host discovery or share discovery in progress. This is only because
  // GatherSharesInNetwork has not been called yet or the previous host
  // discovery has been fully completed.
  InsertDiscoveryAndShareCallbacks(std::move(discovery_callback),
                                   std::move(shares_callback));
  scanner_.FindHostsInNetwork(base::BindOnce(&SmbShareFinder::OnHostsFound,
                                             weak_ptr_factory_.GetWeakPtr()));
}

void SmbShareFinder::DiscoverHostsInNetwork(
    HostDiscoveryResponse discovery_callback) {
  const bool is_host_discovery_pending = !discovery_callbacks_.empty();
  const bool is_share_discovery_pending = !share_callbacks_.empty();

  if (is_host_discovery_pending) {
    // Host discovery is currently running, add |discovery_callback| to
    // |discovery_callbacks|.
    InsertDiscoveryCallback(std::move(discovery_callback));
    return;
  }

  if (is_share_discovery_pending) {
    // Host discovery is complete but there are still share callbacks pending.
    // Run |discovery_callback| because pending share discoveries and no pending
    // host discoveries indicate that a host discovery must have recently
    // completed.
    std::move(discovery_callback).Run();
    return;
  }

  // No host discovery or share discovery in progress. This is only because
  // GatherSharesInNetwork has not been called yet or the previous host
  // discovery has been fully completed.
  InsertDiscoveryCallback(std::move(discovery_callback));
  scanner_.FindHostsInNetwork(base::BindOnce(&SmbShareFinder::OnHostsFound,
                                             weak_ptr_factory_.GetWeakPtr()));
}

void SmbShareFinder::RegisterHostLocator(std::unique_ptr<HostLocator> locator) {
  scanner_.RegisterHostLocator(std::move(locator));
}

SmbUrl SmbShareFinder::GetResolvedUrl(const SmbUrl& url) const {
  DCHECK(url.IsValid());

  const std::string ip_address = scanner_.ResolveHost(url.GetHost()).ToString();
  // Return the original URL if the resolved host cannot be found or if there is
  // no change in the resolved IP address.
  if (ip_address.empty() || ip_address == url.GetHost()) {
    return url;
  }

  return url.ReplaceHost(ip_address);
}

net::IPAddress SmbShareFinder::GetResolvedHost(const std::string& host) const {
  DCHECK(!host.empty());
  return scanner_.ResolveHost(host);
}

bool SmbShareFinder::TryResolveUrl(const SmbUrl& url,
                                   SmbUrl* updated_url) const {
  *updated_url = GetResolvedUrl(url);
  return updated_url->ToString() != url.ToString();
}

void SmbShareFinder::OnHostsFound(bool success, const HostMap& hosts) {
  DCHECK_EQ(0u, host_counter_);

  RunDiscoveryCallbacks();

  if (!success) {
    LOG(ERROR) << "SmbShareFinder failed to find hosts";
    RunEmptySharesCallbacks();
    return;
  }

  if (hosts.empty()) {
    RunEmptySharesCallbacks();
    return;
  }

  host_counter_ = hosts.size();
  for (const auto& host : hosts) {
    const std::string& host_name = host.first;
    const std::string resolved_address = host.second.ToString();
    const base::FilePath server_url(kSmbSchemePrefix + resolved_address);

    client_->GetShares(
        server_url, base::BindOnce(&SmbShareFinder::OnSharesFound,
                                   weak_ptr_factory_.GetWeakPtr(), host_name));
  }
}

void SmbShareFinder::OnSharesFound(
    const std::string& host_name,
    smbprovider::ErrorType error,
    const smbprovider::DirectoryEntryListProto& entries) {
  DCHECK_GT(host_counter_, 0u);
  --host_counter_;

  UMA_HISTOGRAM_ENUMERATION("NativeSmbFileShare.GetSharesResult",
                            TranslateErrorToMountResult(error));

  if (error != smbprovider::ErrorType::ERROR_OK) {
    LOG(ERROR) << "Error finding shares: " << error;
    // Don't early out here because this could be the last host being queried,
    // and the final share discovery callback may need to run.
  }

  for (const smbprovider::DirectoryEntryProto& entry : entries.entries()) {
    SmbUrl url(kSmbSchemePrefix + host_name + "/" + entry.name());
    if (url.IsValid()) {
      shares_.push_back(std::move(url));
    } else {
      LOG(WARNING) << "URL found is not valid";
    }
  }

  if (host_counter_ == 0) {
    RunSharesCallbacks(shares_);
  }
}

void SmbShareFinder::RunDiscoveryCallbacks() {
  for (auto& callback : discovery_callbacks_) {
    std::move(callback).Run();
  }
  discovery_callbacks_.clear();
}

void SmbShareFinder::RunSharesCallbacks(const std::vector<SmbUrl>& shares) {
  for (auto& share_callback : share_callbacks_) {
    std::move(share_callback).Run(shares);
  }
  share_callbacks_.clear();
  shares_.clear();
}

void SmbShareFinder::RunEmptySharesCallbacks() {
  RunSharesCallbacks(std::vector<SmbUrl>());
}

void SmbShareFinder::InsertDiscoveryAndShareCallbacks(
    HostDiscoveryResponse discovery_callback,
    GatherSharesInNetworkResponse share_callback) {
  InsertDiscoveryCallback(std::move(discovery_callback));
  InsertShareCallback(std::move(share_callback));
}

void SmbShareFinder::InsertShareCallback(
    GatherSharesInNetworkResponse share_callback) {
  share_callbacks_.push_back(std::move(share_callback));
}

void SmbShareFinder::InsertDiscoveryCallback(
    HostDiscoveryResponse discovery_callback) {
  discovery_callbacks_.push_back(std::move(discovery_callback));
}

}  // namespace ash::smb_client
