// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_FETCHER_WIN_NETWORK_FETCHER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_FETCHER_WIN_NETWORK_FETCHER_H_

#include "base/functional/callback.h"

class GURL;

namespace enterprise_connectors {

// Interface for the object in charge of issuing a windows key network upload
// request.
class WinNetworkFetcher {
 public:
  // Network request completion callback. The single argument is the response
  // code of the network request.
  using FetchCompletedCallback = base::OnceCallback<void(int)>;

  virtual ~WinNetworkFetcher() = default;

  // Sends a DeviceManagementRequest to the DM server and returns the HTTP
  // response to the `callback`.
  virtual void Fetch(FetchCompletedCallback callback) = 0;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_FETCHER_WIN_NETWORK_FETCHER_H_
