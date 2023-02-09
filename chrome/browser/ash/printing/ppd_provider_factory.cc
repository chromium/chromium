// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/ppd_provider_factory.h"

#include "ash/constants/ash_features.h"
#include "base/files/file_path.h"
#include "base/time/default_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/printing/ppd_cache.h"
#include "chromeos/printing/ppd_metadata_manager.h"
#include "chromeos/printing/ppd_provider.h"
#include "chromeos/printing/printer_config_cache.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/google_api_keys.h"

namespace ash {
namespace {

network::mojom::URLLoaderFactory* GetURLLoaderFactory() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return g_browser_process->system_network_context_manager()
      ->GetURLLoaderFactory();
}

chromeos::PpdIndexChannel ToPpdIndexChannel(
    ash::features::PrintingPpdChannel channel) {
  switch (channel) {
    case ash::features::PrintingPpdChannel::kProduction:
      return chromeos::PpdIndexChannel::kProduction;
    case ash::features::PrintingPpdChannel::kStaging:
      return chromeos::PpdIndexChannel::kStaging;
    case ash::features::PrintingPpdChannel::kDev:
      return chromeos::PpdIndexChannel::kDev;
    case ash::features::PrintingPpdChannel::kLocalhost:
      return chromeos::PpdIndexChannel::kLocalhost;
  }
}

}  // namespace

scoped_refptr<chromeos::PpdProvider> CreatePpdProvider(Profile* profile) {
  const ash::features::PrintingPpdChannel channel =
      ash::features::kPrintingPpdChannelParam.Get();
  const bool use_localhost_as_root =
      (channel == ash::features::PrintingPpdChannel::kLocalhost);
  base::FilePath ppd_cache_path = profile->GetPath().Append(
      use_localhost_as_root ? FILE_PATH_LITERAL("PPDCacheLocalhost")
                            : FILE_PATH_LITERAL("PPDCache"));

  auto provider_config_cache = chromeos::PrinterConfigCache::Create(
      base::DefaultClock::GetInstance(),
      base::BindRepeating(&GetURLLoaderFactory), use_localhost_as_root);

  auto manager_config_cache = chromeos::PrinterConfigCache::Create(
      base::DefaultClock::GetInstance(),
      base::BindRepeating(&GetURLLoaderFactory), use_localhost_as_root);
  auto metadata_manager = chromeos::PpdMetadataManager::Create(
      g_browser_process->GetApplicationLocale(), ToPpdIndexChannel(channel),
      base::DefaultClock::GetInstance(), std::move(manager_config_cache));

  return chromeos::PpdProvider::Create(
      version_info::GetVersion(), chromeos::PpdCache::Create(ppd_cache_path),
      std::move(metadata_manager), std::move(provider_config_cache));
}

}  // namespace ash
