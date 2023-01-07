// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_avatar_downloader.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/referrer_policy.h"
#include "ui/gfx/image/image.h"

namespace {
const char kHighResAvatarDownloadUrlPrefix[] =
    "https://www.gstatic.com/chrome/profile_avatars/";
}

ProfileAvatarDownloader::ProfileAvatarDownloader(size_t icon_index,
                                                 FetchCompleteCallback callback)
    : icon_index_(icon_index), callback_(std::move(callback)) {
  DCHECK(!callback_.is_null());
  GURL url(std::string(kHighResAvatarDownloadUrlPrefix) +
           profiles::GetDefaultAvatarIconFileNameAtIndex(icon_index));
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("profile_avatar", R"(
        semantics {
          sender: "Profile Avatar Downloader"
          description:
            "The Chromium binary comes with a bundle of low-resolution "
            "versions of avatar images. When the user selects an avatar in "
            "chrome://settings, Chromium will download a high-resolution "
            "version from Google's static content servers for use in the "
            "people manager UI."
          trigger:
            "User selects a new avatar in chrome://settings for their profile"
          data: "None, only the filename of the png to download."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled in settings."
          policy_exception_justification:
            "No content is being uploaded or saved; this request merely "
            "downloads a publicly available PNG file."
        })");
  fetcher_ = std::make_unique<BitmapFetcher>(url, this, traffic_annotation);
}

ProfileAvatarDownloader::~ProfileAvatarDownloader() = default;

void ProfileAvatarDownloader::Start() {
  SystemNetworkContextManager* system_network_context_manager =
      g_browser_process->system_network_context_manager();
  // In unit tests, the browser process can return a NULL context manager
  if (!system_network_context_manager)
    return;
  network::mojom::URLLoaderFactory* loader_factory =
      system_network_context_manager->GetURLLoaderFactory();
  if (loader_factory) {
    fetcher_->Init(
        net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN,
        network::mojom::CredentialsMode::kOmit);
    fetcher_->Start(loader_factory);
  }
}

// BitmapFetcherDelegate overrides.
void ProfileAvatarDownloader::OnFetchComplete(const GURL& url,
                                              const SkBitmap* bitmap) {
  if (!bitmap)
    return;

  // Decode the downloaded bitmap. Ownership of the image is taken by |cache_|.
  std::move(callback_).Run(
      gfx::Image::CreateFrom1xBitmap(*bitmap),
      profiles::GetDefaultAvatarIconFileNameAtIndex(icon_index_),
      profiles::GetPathOfHighResAvatarAtIndex(icon_index_));
}
