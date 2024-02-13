// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/smb_client/discovery/network_scanner.h"

#include <map>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/smb_client/discovery/host_locator.h"

namespace ash::smb_client {

namespace {

// Returns true if the host with |host_name| already exists in |host_map|.
bool HostExists(const HostMap& host_map, const Hostname& host_name) {
  return host_map.count(host_name);
}

}  // namespace

RequestInfo::RequestInfo(uint32_t remaining_requests,
                         FindHostsCallback callback)
    : remaining_requests(remaining_requests), callback(std::move(callback)) {}

RequestInfo::RequestInfo(RequestInfo&& other) = default;

RequestInfo::~RequestInfo() = default;

NetworkScanner::NetworkScanner() = default;

NetworkScanner::~NetworkScanner() = default;

void NetworkScanner::FindHostsInNetwork(FindHostsCallback callback) {
  DCHECK(!running_);

  if (locators_.empty()) {
    find_hosts_returned_ = true;
    // Fire the callback immediately if there are no registered HostLocators.
    std::move(callback).Run(false /* success */, HostMap());
    return;
  }

  running_ = true;

  const uint32_t request_id = AddNewRequest(std::move(callback));
  for (const auto& locator : locators_) {
    locator->FindHosts(base::BindOnce(&NetworkScanner::OnHostsFound,
                                      weak_ptr_factory_.GetWeakPtr(),
                                      request_id));
  }
}

void NetworkScanner::RegisterHostLocator(std::unique_ptr<HostLocator> locator) {
  locators_.push_back(std::move(locator));
}

net::IPAddress NetworkScanner::ResolveHost(const std::string& host) const {
  DCHECK(find_hosts_returned_);

  const auto& host_iter = found_hosts_.find(base::ToLowerASCII(host));
  if (host_iter == found_hosts_.end()) {
    return {};
  }

  return host_iter->second;
}

void NetworkScanner::OnHostsFound(uint32_t request_id,
                                  bool success,
                                  const HostMap& host_map) {
  DCHECK_GT(requests_.count(request_id), 0u);

  if (success) {
    AddHostsToResults(request_id, host_map);
  }

  FireCallbackIfFinished(request_id);
}

void NetworkScanner::AddHostsToResults(uint32_t request_id,
                                       const HostMap& new_hosts) {
  auto request_iter = requests_.find(request_id);
  DCHECK(request_iter != requests_.end());

  HostMap& existing_hosts = request_iter->second.hosts_found;
  for (const auto& new_host : new_hosts) {
    const Hostname new_hostname = base::ToLowerASCII(new_host.first);
    const Address& new_ip = new_host.second;

    if (!HostExists(existing_hosts, new_hostname)) {
      existing_hosts.insert(std::pair<Hostname, Address>(new_hostname, new_ip));
    } else if (existing_hosts[new_hostname] != new_ip) {
      LOG(WARNING) << "Different addresses found for host: " << new_hostname;
      LOG(WARNING) << "Existing " << existing_hosts[new_hostname].ToString()
                   << " new " << new_ip.ToString();
    }
  }
}

uint32_t NetworkScanner::AddNewRequest(FindHostsCallback callback) {
  const uint32_t request_id = next_request_id_++;
  requests_.emplace(request_id,
                    RequestInfo(locators_.size(), std::move(callback)));
  return request_id;
}

void NetworkScanner::FireCallbackIfFinished(uint32_t request_id) {
  auto request_iter = requests_.find(request_id);
  DCHECK(request_iter != requests_.end());

  uint32_t& remaining_requests = request_iter->second.remaining_requests;
  DCHECK_GT(remaining_requests, 0u);

  if (--remaining_requests == 0) {
    RequestInfo info = std::move(request_iter->second);
    requests_.erase(request_iter);

    // Save the found hosts for name resolution.
    found_hosts_ = std::move(info.hosts_found);
    find_hosts_returned_ = true;

    running_ = false;
    std::move(info.callback).Run(true /* success */, found_hosts_);
  }
}

}  // namespace ash::smb_client
