// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "chrome/browser/chromeos/login/screens/recommend_apps/recommend_apps_fetcher.h"

#include "base/callback.h"
#include "chrome/browser/chromeos/login/screens/recommend_apps/recommend_apps_fetcher_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/system_connector.h"

namespace chromeos {

namespace {

// The factory callback that will be used to create RecommendAppsFetcher
// instances other than default RecommendAppsFetcherImpl.
// It can be set by SetFactoryCallbackForTesting().
RecommendAppsFetcher::FactoryCallback* g_factory_callback = nullptr;

}  // namespace

// static
std::unique_ptr<RecommendAppsFetcher> RecommendAppsFetcher::Create(
    RecommendAppsFetcherDelegate* delegate) {
  if (g_factory_callback)
    return g_factory_callback->Run(delegate);
  return std::make_unique<RecommendAppsFetcherImpl>(
      delegate, content::GetSystemConnector(),
      content::BrowserContext::GetDefaultStoragePartition(
          ProfileManager::GetActiveUserProfile())
          ->GetURLLoaderFactoryForBrowserProcess()
          .get());
}

// static
void RecommendAppsFetcher::SetFactoryCallbackForTesting(
    FactoryCallback* callback) {
  DCHECK(!g_factory_callback || !callback);

  g_factory_callback = callback;
}

}  // namespace chromeos
