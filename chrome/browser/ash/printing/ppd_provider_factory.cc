// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/ppd_provider_factory.h"

#include <string>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/time/default_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/printing/ppd_cache.h"
#include "chromeos/printing/ppd_metadata_manager.h"
#include "chromeos/printing/ppd_provider.h"
#include "chromeos/printing/printer_config_cache.h"
#include "chromeos/printing/remote_ppd_fetcher.h"
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

chromeos::PpdIndexChannel ToPpdIndexChannel(const std::string& channel) {
  if (channel == ash::switches::kPrintingPpdChannelStaging) {
    return chromeos::PpdIndexChannel::kStaging;
  }
  if (channel == ash::switches::kPrintingPpdChannelDev) {
    return chromeos::PpdIndexChannel::kDev;
  }
  if (channel == ash::switches::kPrintingPpdChannelLocalhost) {
    return chromeos::PpdIndexChannel::kLocalhost;
  }
  return chromeos::PpdIndexChannel::kProduction;
}

}  // namespace

scoped_refptr<chromeos::PpdProvider> CreatePpdProvider(Profile* profile) {
  const chromeos::PpdIndexChannel channel = ToPpdIndexChannel(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          ash::switches::kPrintingPpdChannel));

  const bool use_localhost_as_root =
      (channel == chromeos::PpdIndexChannel::kLocalhost);
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
      channel, base::DefaultClock::GetInstance(),
      std::move(manager_config_cache));

  auto remote_ppd_fetcher = chromeos::RemotePpdFetcher::Create(
      base::BindRepeating(&GetURLLoaderFactory));

  return chromeos::PpdProvider::Create(
      version_info::GetVersion(), chromeos::PpdCache::Create(ppd_cache_path),
      std::move(metadata_manager), std::move(provider_config_cache),
      std::move(remote_ppd_fetcher));
}

}  // namespace ash
