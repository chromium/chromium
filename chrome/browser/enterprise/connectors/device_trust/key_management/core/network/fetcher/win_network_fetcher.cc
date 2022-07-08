// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/fetcher/win_network_fetcher.h"

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/fetcher/win_network_fetcher_impl.h"

namespace enterprise_connectors {

namespace {

std::unique_ptr<WinNetworkFetcher>* GetTestInstanceStorage() {
  static base::NoDestructor<std::unique_ptr<WinNetworkFetcher>> storage;
  return storage.get();
}

}  // namespace

// static
std::unique_ptr<WinNetworkFetcher> WinNetworkFetcher::Create(
    const GURL& url,
    const std::string& body,
    base::flat_map<std::string, std::string>& headers) {
  std::unique_ptr<WinNetworkFetcher>& test_instance = *GetTestInstanceStorage();
  if (test_instance)
    return std::move(test_instance);
  return std::make_unique<WinNetworkFetcherImpl>(url, body, headers);
}

// static
void WinNetworkFetcher::SetInstanceForTesting(
    std::unique_ptr<WinNetworkFetcher> fetcher) {
  DCHECK(fetcher);
  *GetTestInstanceStorage() = std::move(fetcher);
}

}  // namespace enterprise_connectors
