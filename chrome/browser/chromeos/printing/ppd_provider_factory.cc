// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/ppd_provider_factory.h"

#include "base/files/file_path.h"
#include "base/time/default_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/printing/ppd_cache.h"
#include "chromeos/printing/ppd_provider_v3.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/google_api_keys.h"

namespace chromeos {
namespace {

network::mojom::URLLoaderFactory* GetURLLoaderFactory() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return g_browser_process->system_network_context_manager()
      ->GetURLLoaderFactory();
}

}  // namespace

scoped_refptr<PpdProvider> CreatePpdProvider(Profile* profile) {
  base::FilePath ppd_cache_path =
      profile->GetPath().Append(FILE_PATH_LITERAL("PPDCache"));

  auto provider_config_cache =
      PrinterConfigCache::Create(base::DefaultClock::GetInstance(),
                                 base::BindRepeating(&GetURLLoaderFactory));

  auto manager_config_cache =
      PrinterConfigCache::Create(base::DefaultClock::GetInstance(),
                                 base::BindRepeating(&GetURLLoaderFactory));
  auto metadata_manager = PpdMetadataManager::Create(
      g_browser_process->GetApplicationLocale(),
      base::DefaultClock::GetInstance(), std::move(manager_config_cache));

  return CreateV3Provider(
      version_info::GetVersion(), PpdCache::Create(ppd_cache_path),
      std::move(metadata_manager), std::move(provider_config_cache));
}

}  // namespace chromeos
