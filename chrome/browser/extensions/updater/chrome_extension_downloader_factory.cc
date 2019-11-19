// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/updater/chrome_extension_downloader_factory.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/extensions/updater/extension_updater_switches.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/crx_file/crx_verifier.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/update_client/update_query_params.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/updater/extension_downloader.h"
#include "extensions/common/verifier_formats.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

using extensions::ExtensionDownloader;
using extensions::ExtensionDownloaderDelegate;
using update_client::UpdateQueryParams;

std::unique_ptr<ExtensionDownloader>
ChromeExtensionDownloaderFactory::CreateForURLLoaderFactory(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    ExtensionDownloaderDelegate* delegate,
    crx_file::VerifierFormat required_verifier_format,
    const base::FilePath& profile_path) {
  std::unique_ptr<ExtensionDownloader> downloader(
      new ExtensionDownloader(delegate, std::move(url_loader_factory),
                              required_verifier_format, profile_path));
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  std::string brand;
  google_brand::GetBrand(&brand);
  if (!google_brand::IsOrganic(brand))
    downloader->set_brand_code(brand);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  std::string manifest_query_params =
      UpdateQueryParams::Get(UpdateQueryParams::CRX);
  base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(extensions::kSwitchTestRequestParam)) {
    manifest_query_params += "&testrequest=1";
  }
  downloader->set_manifest_query_params(manifest_query_params);
  downloader->set_ping_enabled_domain("google.com");
  return downloader;
}

std::unique_ptr<ExtensionDownloader>
ChromeExtensionDownloaderFactory::CreateForProfile(
    Profile* profile,
    ExtensionDownloaderDelegate* delegate) {
  std::unique_ptr<ExtensionDownloader> downloader = CreateForURLLoaderFactory(
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetURLLoaderFactoryForBrowserProcess(),
      delegate, extensions::GetPolicyVerifierFormat(), profile->GetPath());

  // NOTE: It is not obvious why it is OK to pass raw pointers to the token
  // service and identity manager here. The logic is as follows:
  // ExtensionDownloader is owned by ExtensionUpdater.
  // ExtensionUpdater is owned by ExtensionService.
  // ExtensionService is owned by ExtensionSystemImpl::Shared.
  // ExtensionSystemImpl::Shared is a KeyedService. Its factory
  // (ExtensionSystemSharedFactory) specifies that it depends on
  // IdentityManager. Hence, the IdentityManager instance is guaranteed to
  // outlive |downloader|.
  // TODO(843519): Make this lifetime relationship more explicit/cleaner.
  downloader->SetIdentityManager(
      IdentityManagerFactory::GetForProfile(profile));
  return downloader;
}
