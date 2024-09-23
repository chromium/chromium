// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/customization/customization_wallpaper_downloader.h"

#include <math.h>
#include <algorithm>
#include <utility>

#include "base/files/file_util.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace ash {
namespace {
// This is temporary file suffix (for downloading or resizing).
const char kTemporarySuffix[] = ".tmp";

// Sleep between wallpaper retries (used multiplied by squared retry number).
const unsigned kRetrySleepSeconds = 10;

// Retry is infinite with increasing intervals. When calculated delay becomes
// longer than maximum (kMaxRetrySleepSeconds) it is set to the maximum.
const double kMaxRetrySleepSeconds = 6 * 3600;  // 6 hours

constexpr net::NetworkTrafficAnnotationTag
    kCustomizationWallPaperDownloaderNetworkTag =
        net::DefineNetworkTrafficAnnotation(
            "customization_wallpaper_downloader",
            R"(
        semantics {
          sender: "Customization wallpaper"
          description:
            "Download wallpaper from OEM custom url to the wallpaper directory "
            "during OOBE. Admin/user can override the OEM wallpaper to have a "
            "custom wallpaper. If the admin/user set a custom wallpaper, after "
            "user sign in, the user will see their preferred wallpaper."
          trigger:
            "Triggered to get the OEM's custom wallpaper on device bootup."
            "The downloaded custom wallpaper is stored until powerwash."
          data: "None."
          destination: WEBSITE
        }
        policy {
         cookies_allowed: NO
         setting:
           "This feature is set by OEMs and can be overridden by users "
           "after sign in."
         policy_exception_justification:
           "This request is made based on OEM customization and does not "
           "send/store any sensitive data."
        })");

void CreateWallpaperDirectory(const base::FilePath& wallpaper_dir,
                              bool* success) {
  DCHECK(success);

  *success = CreateDirectoryAndGetError(wallpaper_dir, nullptr);
  if (!*success) {
    NOTREACHED_IN_MIGRATION()
        << "Failed to create directory '" << wallpaper_dir.value() << "'";
  }
}

void RenameTemporaryFile(const base::FilePath& from,
                         const base::FilePath& to,
                         bool* success) {
  DCHECK(success);

  base::File::Error error;
  if (base::ReplaceFile(from, to, &error)) {
    *success = true;
  } else {
    LOG(WARNING)
        << "Failed to rename temporary file of Customized Wallpaper. error="
        << error;
    *success = false;
  }
}

}  // namespace

CustomizationWallpaperDownloader::CustomizationWallpaperDownloader(
    const GURL& wallpaper_url,
    const base::FilePath& wallpaper_dir,
    const base::FilePath& wallpaper_downloaded_file,
    base::OnceCallback<void(bool success, const GURL&)>
        on_wallpaper_fetch_completed)
    : wallpaper_url_(wallpaper_url),
      wallpaper_dir_(wallpaper_dir),
      wallpaper_downloaded_file_(wallpaper_downloaded_file),
      wallpaper_temporary_file_(wallpaper_downloaded_file.value() +
                                kTemporarySuffix),
      retries_(0),
      retry_delay_(base::Seconds(kRetrySleepSeconds)),
      on_wallpaper_fetch_completed_(std::move(on_wallpaper_fetch_completed)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

CustomizationWallpaperDownloader::~CustomizationWallpaperDownloader() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void CustomizationWallpaperDownloader::StartRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(wallpaper_url_.is_valid());

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = wallpaper_url_;
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  simple_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), kCustomizationWallPaperDownloaderNetworkTag);

  SystemNetworkContextManager* system_network_context_manager =
      g_browser_process->system_network_context_manager();
  // In unit tests, the browser process can return a null context manager
  if (!system_network_context_manager)
    return;

  network::mojom::URLLoaderFactory* loader_factory =
      system_network_context_manager->GetURLLoaderFactory();

  simple_loader_->DownloadToFile(
      loader_factory,
      base::BindOnce(&CustomizationWallpaperDownloader::OnSimpleLoaderComplete,
                     base::Unretained(this)),
      wallpaper_temporary_file_);
}

void CustomizationWallpaperDownloader::Retry() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ++retries_;

  const double delay_seconds = std::min(
      kMaxRetrySleepSeconds,
      static_cast<double>(retries_) * retries_ * retry_delay_.InSecondsF());
  const base::TimeDelta delay = base::Seconds(delay_seconds);

  VLOG(1) << "Schedule Customized Wallpaper download in " << delay.InSecondsF()
          << " seconds (retry = " << retries_ << ").";
  retry_current_delay_ = delay;
  request_scheduled_.Start(
      FROM_HERE, delay, this, &CustomizationWallpaperDownloader::StartRequest);
}

void CustomizationWallpaperDownloader::Start() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::unique_ptr<bool> success(new bool(false));

  base::OnceClosure mkdir_closure =
      base::BindOnce(&CreateWallpaperDirectory, wallpaper_dir_,
                     base::Unretained(success.get()));
  base::OnceClosure on_created_closure = base::BindOnce(
      &CustomizationWallpaperDownloader::OnWallpaperDirectoryCreated,
      weak_factory_.GetWeakPtr(), std::move(success));
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      std::move(mkdir_closure), std::move(on_created_closure));
}

void CustomizationWallpaperDownloader::OnWallpaperDirectoryCreated(
    std::unique_ptr<bool> success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (*success)
    StartRequest();
}

void CustomizationWallpaperDownloader::OnSimpleLoaderComplete(
    base::FilePath response_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const bool error = response_path.empty();

  VLOG(1) << "CustomizationWallpaperDownloader::OnURLFetchComplete(): status="
          << simple_loader_->NetError();

  simple_loader_.reset();

  if (error) {
    Retry();
    return;
  }

  std::unique_ptr<bool> success(new bool(false));

  base::OnceClosure rename_closure = base::BindOnce(
      &RenameTemporaryFile, response_path, wallpaper_downloaded_file_,
      base::Unretained(success.get()));
  base::OnceClosure on_rename_closure =
      base::BindOnce(&CustomizationWallpaperDownloader::OnTemporaryFileRenamed,
                     weak_factory_.GetWeakPtr(), std::move(success));
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      std::move(rename_closure), std::move(on_rename_closure));
}

void CustomizationWallpaperDownloader::OnTemporaryFileRenamed(
    std::unique_ptr<bool> success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::move(on_wallpaper_fetch_completed_).Run(*success, wallpaper_url_);
}

}  // namespace ash
