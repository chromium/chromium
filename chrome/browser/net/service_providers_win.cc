// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/service_providers_win.h"

#include <winsock2.h>
#include <Ws2spi.h>

#include <memory>

#include "base/notreached.h"
#include "base/values.h"

WinsockLayeredServiceProvider::WinsockLayeredServiceProvider() {
}

WinsockLayeredServiceProvider::WinsockLayeredServiceProvider(
    const WinsockLayeredServiceProvider& other) = default;

WinsockLayeredServiceProvider::~WinsockLayeredServiceProvider() {
}

void GetWinsockNamespaceProviders(
    WinsockNamespaceProviderList* namespace_list) {

  // Find out how just how much memory is needed.  If we get the expected error,
  // the memory needed is written to size.
  DWORD size = 0;
  if (WSAEnumNameSpaceProviders(&size, NULL) != SOCKET_ERROR ||
      GetLastError() != WSAEFAULT) {
    NOTREACHED();
    return;
  }

  std::unique_ptr<char[]> namespace_provider_bytes(new char[size]);
  WSANAMESPACE_INFO* namespace_providers =
      reinterpret_cast<WSANAMESPACE_INFO*>(namespace_provider_bytes.get());

  int num_namespace_providers = WSAEnumNameSpaceProviders(&size,
                                                          namespace_providers);
  if (num_namespace_providers == SOCKET_ERROR) {
    NOTREACHED();
    return;
  }

  for (int i = 0; i < num_namespace_providers; ++i) {
    WinsockNamespaceProvider provider;

    provider.name = namespace_providers[i].lpszIdentifier;
    provider.active = TRUE == namespace_providers[i].fActive;
    provider.version = namespace_providers[i].dwVersion;
    provider.type = namespace_providers[i].dwNameSpace;

    namespace_list->push_back(provider);
  }
}

void GetWinsockLayeredServiceProviders(
    WinsockLayeredServiceProviderList* service_list) {
  // Find out how just how much memory is needed.  If we get the expected error,
  // the memory needed is written to size.
  DWORD size = 0;
  int error;
  if (SOCKET_ERROR != WSCEnumProtocols(NULL, NULL, &size, &error) ||
      error != WSAENOBUFS) {
    NOTREACHED();
    return;
  }

  std::unique_ptr<char[]> service_provider_bytes(new char[size]);
  WSAPROTOCOL_INFOW* service_providers =
      reinterpret_cast<WSAPROTOCOL_INFOW*>(service_provider_bytes.get());

  int num_service_providers = WSCEnumProtocols(NULL, service_providers, &size,
                                               &error);
  if (num_service_providers == SOCKET_ERROR) {
    NOTREACHED();
    return;
  }

  for (int i = 0; i < num_service_providers; ++i) {
    WinsockLayeredServiceProvider service_provider;

    service_provider.name = service_providers[i].szProtocol;
    service_provider.version = service_providers[i].iVersion;
    service_provider.socket_type = service_providers[i].iSocketType;
    service_provider.socket_protocol = service_providers[i].iProtocol;
    service_provider.chain_length = service_providers[i].ProtocolChain.ChainLen;

    // TODO(mmenke): Add categories under Vista and later.
    // http://msdn.microsoft.com/en-us/library/ms742239%28v=VS.85%29.aspx

    wchar_t path[MAX_PATH];
    int path_length = std::size(path);
    if (0 == WSCGetProviderPath(&service_providers[i].ProviderId, path,
                                &path_length, &error)) {
      service_provider.path = path;
    }

    service_list->push_back(service_provider);
  }

  return;
}

