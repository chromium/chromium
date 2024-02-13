// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SMB_CLIENT_SMB_SHARE_FINDER_H_
#define CHROME_BROWSER_ASH_SMB_CLIENT_SMB_SHARE_FINDER_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/smb_client/discovery/host_locator.h"
#include "chrome/browser/ash/smb_client/discovery/network_scanner.h"
#include "chrome/browser/ash/smb_client/smb_url.h"
#include "chromeos/ash/components/dbus/smbprovider/smb_provider_client.h"

namespace ash::smb_client {

// The callback run to indicate the scan for hosts on the network is complete.
using HostDiscoveryResponse = base::OnceClosure;

// This class is responsible for finding hosts in a network and getting the
// available shares for each host found.
class SmbShareFinder final {
 public:
  // The callback that will be passed to GatherSharesInNetwork.
  using GatherSharesInNetworkResponse =
      base::OnceCallback<void(const std::vector<SmbUrl>& shares_gathered)>;

  explicit SmbShareFinder(SmbProviderClient* client);
  SmbShareFinder(const SmbShareFinder&) = delete;
  SmbShareFinder& operator=(const SmbShareFinder&) = delete;
  ~SmbShareFinder();

  // Gathers the hosts in the network using |scanner_| and gets the shares for
  // each of the hosts found. |discovery_callback| runs once when host
  // disovery is complete. |shares_callback| only runs once when all entries
  // from hosts are stored to |shares| and will contain the paths to the shares
  // found (e.g. "smb://host/share").
  void GatherSharesInNetwork(HostDiscoveryResponse discovery_callback,
                             GatherSharesInNetworkResponse shares_callback);

  // Gathers the hosts in the network using |scanner_|. Runs
  // |discovery_callback| upon completion. No data is returned to the caller,
  // but hosts are cached in |scanner_| and can be used for name resolution.
  void DiscoverHostsInNetwork(HostDiscoveryResponse discovery_callback);

  // Registers HostLocator |locator| to |scanner_|.
  void RegisterHostLocator(std::unique_ptr<HostLocator> locator);

  // Attempts to resolve |url|. Returns the resolved url if successful,
  // otherwise returns |url|.
  SmbUrl GetResolvedUrl(const SmbUrl& url) const;

  // Returns the IP address of |host|. |host| MUST be non-empty. If |host|
  // cannot be resolved, returns the empty/invalid IPAddress.
  net::IPAddress GetResolvedHost(const std::string& host) const;

  // Attempts to resolve |url|. If able to resolve |url|, returns true and sets
  // |resolved_url| the the resolved url. If unable, returns false and sets
  // |resolved_url| to |url|.
  bool TryResolveUrl(const SmbUrl& url, SmbUrl* updated_url) const;

 private:
  // Handles the response from finding hosts in the network.
  void OnHostsFound(bool success, const HostMap& hosts);

  // Handles the response from getting shares for a given host.
  void OnSharesFound(const std::string& host_name,
                     smbprovider::ErrorType error,
                     const smbprovider::DirectoryEntryListProto& entries);

  // Executes all the DiscoveryCallbacks inside |discovery_callbacks_|.
  void RunDiscoveryCallbacks();

  // Executes all the SharesCallback inside |shares_callback_|.
  void RunSharesCallbacks(const std::vector<SmbUrl>& shares);

  // Executes all the SharesCallback inside |shares_callback_| with an empty
  // vector of SmbUrl.
  void RunEmptySharesCallbacks();

  // Inserts HostDiscoveryResponse in |discovery_callbacks_| and inserts
  // GatherSharesInNetworkResponse in |shares_callbacks_|.
  void InsertDiscoveryAndShareCallbacks(
      HostDiscoveryResponse discovery_callback,
      GatherSharesInNetworkResponse shares_callback);

  // Inserts |shares_callback| to |share_callbacks_|.
  void InsertShareCallback(GatherSharesInNetworkResponse shares_callback);

  // Inserts |discovery_callback| to |discovery_callbacks_|.
  void InsertDiscoveryCallback(HostDiscoveryResponse discovery_callback);

  NetworkScanner scanner_;

  raw_ptr<SmbProviderClient, DanglingUntriaged> client_;  // Not owned.

  uint32_t host_counter_ = 0u;

  std::vector<HostDiscoveryResponse> discovery_callbacks_;
  std::vector<GatherSharesInNetworkResponse> share_callbacks_;

  std::vector<SmbUrl> shares_;
  base::WeakPtrFactory<SmbShareFinder> weak_ptr_factory_{this};
};

}  // namespace ash::smb_client

#endif  // CHROME_BROWSER_ASH_SMB_CLIENT_SMB_SHARE_FINDER_H_
