// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SMB_CLIENT_DISCOVERY_NETWORK_SCANNER_H_
#define CHROME_BROWSER_ASH_SMB_CLIENT_DISCOVERY_NETWORK_SCANNER_H_

#include <map>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/smb_client/discovery/host_locator.h"
#include "net/base/ip_address.h"

namespace ash::smb_client {

// Holds the number of in-flight requests and the callback to call once all the
// HostLocators are finished. Also holds the hosts found from the HostLocators
// that have already returned.
struct RequestInfo {
  uint32_t remaining_requests;
  FindHostsCallback callback;
  HostMap hosts_found;

  RequestInfo(uint32_t remaining_requests, FindHostsCallback callback);
  RequestInfo(RequestInfo&& other);

  RequestInfo(const RequestInfo&) = delete;
  RequestInfo& operator=(const RequestInfo&) = delete;

  ~RequestInfo();
};

// NetworkScanner discovers SMB hosts in the local network by querying
// registered HostLocators and aggregating their results. RegisterHostLocator is
// used to register HostLocators that are responsible for finding hosts.
// FindHostsInNetwork is called to get a list of discoverable hosts in the
// network. ResolveHost is used to get the IP address of a given host.
class NetworkScanner {
 public:
  NetworkScanner();

  NetworkScanner(const NetworkScanner&) = delete;
  NetworkScanner& operator=(const NetworkScanner&) = delete;

  ~NetworkScanner();

  // Query the registered HostLocators and return all the hosts found.
  // |callback| is called once all the HostLocators have responded with their
  // results. If there are no locators, the callback is fired immediately with
  // an empty result and success set to false. Once this call has returned, the
  // hosts found are cached locally and are resolvable individually through
  // ResolveHost().
  void FindHostsInNetwork(FindHostsCallback callback);

  // Registeres a |locator| to be queried when FindHostsInNetwork() is called.
  void RegisterHostLocator(std::unique_ptr<HostLocator> locator);

  // Resolves |host| to an address using the cached results of
  // FindHostsInNetwork(). FindHostsInNetwork() has to be called beforehand. If
  // no address is found, this returns an invalid IPAddress.
  net::IPAddress ResolveHost(const std::string& host) const;

 private:
  // Callback handler for HostLocator::FindHosts().
  void OnHostsFound(uint32_t request_id, bool success, const HostMap& host_map);

  // Adds |host_map| hosts to current results. The host will not be added if the
  // hostname already exists in results, and if the IP address does not match,
  // it will be logged.
  void AddHostsToResults(uint32_t request_id, const HostMap& host_map);

  // Adds a new request to track and saves |callback| to be called when the
  // request is finished. Returns the request id.
  uint32_t AddNewRequest(FindHostsCallback callback);

  // Called after a HostLocator returns with results and decrements the count of
  // requests in RequestInfo for |request_id|. Fires the callback for if
  // there are no more requests and deletes the corresponding RequestInfo.
  void FireCallbackIfFinished(uint32_t request_id);

  std::vector<std::unique_ptr<HostLocator>> locators_;

  // Used for tracking in-flight requests to HostLocators. The key is the
  // request id, and the value is the RequestInfo struct.
  std::map<uint32_t, RequestInfo> requests_;

  uint32_t next_request_id_ = 0;

  // Hosts that are found from FindHostsInNetwork(). This is cached for name
  // resolution when calling ResolveHost().
  HostMap found_hosts_;

  // True if FindHostsInNetwork() has been called and returned results
  // regardless if any hosts are found.
  bool find_hosts_returned_ = false;

  // True if FindHostsInNetwork() has been called and is waiting for
  // FindHostsCallback to be invoked. This is to prevent multiple calls of
  // FindHostsInNetwork() from concurrently executing. Used only for DCHECKing
  // if FindHostsInNetwork() is already running.
  bool running_ = false;

  base::WeakPtrFactory<NetworkScanner> weak_ptr_factory_{this};
};

}  // namespace ash::smb_client

#endif  // CHROME_BROWSER_ASH_SMB_CLIENT_DISCOVERY_NETWORK_SCANNER_H_
