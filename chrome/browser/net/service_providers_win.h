// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SERVICE_PROVIDERS_WIN_H_
#define CHROME_BROWSER_NET_SERVICE_PROVIDERS_WIN_H_

#include <string>
#include <vector>

struct WinsockNamespaceProvider {
  std::wstring name;
  int version;
  bool active;
  int type;
};
using WinsockNamespaceProviderList = std::vector<WinsockNamespaceProvider>;

struct WinsockLayeredServiceProvider {
  WinsockLayeredServiceProvider();
  WinsockLayeredServiceProvider(const WinsockLayeredServiceProvider& other);
  ~WinsockLayeredServiceProvider();

  std::wstring name;
  std::wstring path;
  int version;
  int chain_length;
  int socket_type;
  int socket_protocol;
};
using WinsockLayeredServiceProviderList =
    std::vector<WinsockLayeredServiceProvider>;

// Returns all the Winsock namespace providers.
void GetWinsockNamespaceProviders(WinsockNamespaceProviderList* namespace_list);

// Returns all the Winsock layered service providers and their paths.
void GetWinsockLayeredServiceProviders(
    WinsockLayeredServiceProviderList* service_list);

#endif  // CHROME_BROWSER_NET_SERVICE_PROVIDERS_WIN_H_
