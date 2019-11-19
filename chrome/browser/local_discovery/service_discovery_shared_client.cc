// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/service_discovery_shared_client.h"

#include <memory>

#include "build/build_config.h"
#include "net/net_buildflags.h"

#if defined(OS_MACOSX)
#include "chrome/browser/local_discovery/service_discovery_client_mac_factory.h"
#endif

#if BUILDFLAG(ENABLE_MDNS)
#include "base/memory/ref_counted.h"
#include "chrome/browser/local_discovery/service_discovery_client_mdns.h"
#endif

namespace local_discovery {

using content::BrowserThread;

namespace {

ServiceDiscoverySharedClient* g_service_discovery_client = nullptr;

}  // namespace

ServiceDiscoverySharedClient::ServiceDiscoverySharedClient() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!g_service_discovery_client);
  g_service_discovery_client = this;
}

ServiceDiscoverySharedClient::~ServiceDiscoverySharedClient() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(g_service_discovery_client, this);
  g_service_discovery_client = nullptr;
}

// static
scoped_refptr<ServiceDiscoverySharedClient>
    ServiceDiscoverySharedClient::GetInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if BUILDFLAG(ENABLE_MDNS) || defined(OS_MACOSX)
  if (g_service_discovery_client)
    return g_service_discovery_client;

#if defined(OS_MACOSX)
  return ServiceDiscoveryClientMacFactory::CreateInstance();
#else
  return base::MakeRefCounted<ServiceDiscoveryClientMdns>();
#endif  // defined(OS_MACOSX)
#else
  NOTIMPLEMENTED();
  return nullptr;
#endif  // BUILDFLAG(ENABLE_MDNS) || defined(OS_MACOSX)
}

}  // namespace local_discovery
