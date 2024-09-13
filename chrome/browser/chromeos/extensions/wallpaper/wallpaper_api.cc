// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/wallpaper/wallpaper_api.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/features/feature.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/wallpaper.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#else
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/wallpaper_ash.h"
#endif

using base::Value;
using content::BrowserThread;

using FetchCallback =
    base::OnceCallback<void(bool success, const std::string&)>;

namespace set_wallpaper = extensions::api::wallpaper::SetWallpaper;

namespace {

crosapi::mojom::WallpaperLayout GetMojoLayoutEnum(
    extensions::api::wallpaper::WallpaperLayout layout) {
  switch (layout) {
    case extensions::api::wallpaper::WallpaperLayout::kStretch:
      return crosapi::mojom::WallpaperLayout::kStretch;
    case extensions::api::wallpaper::WallpaperLayout::kCenter:
      return crosapi::mojom::WallpaperLayout::kCenter;
    case extensions::api::wallpaper::WallpaperLayout::kCenterCropped:
      return crosapi::mojom::WallpaperLayout::kCenterCropped;
    default:
      return crosapi::mojom::WallpaperLayout::kCenter;
  }
}

crosapi::mojom::Wallpaper* GetWallpaperApi() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service->IsAvailable<crosapi::mojom::Wallpaper>()) {
    return nullptr;
  }
  return lacros_service->GetRemote<crosapi::mojom::Wallpaper>().get();
#else
  return crosapi::CrosapiManager::Get()->crosapi_ash()->wallpaper_ash();
#endif
}

class WallpaperFetcher {
 public:
  WallpaperFetcher() {}

  static const char kCancelWallpaperMessage[];

  void FetchWallpaper(const GURL& url, FetchCallback callback) {
    CancelPreviousFetch();
    original_url_ = url;
    callback_ = std::move(callback);

    net::NetworkTrafficAnnotationTag traffic_annotation =
        net::DefineNetworkTrafficAnnotation("wallpaper_fetcher", R"(
          semantics {
            sender: "Wallpaper Fetcher"
            description:
              "Chrome OS downloads wallpaper upon user request."
            trigger:
              "When an app or extension requests to download "
              "a wallpaper from a remote URL."
            data:
              "User-selected image."
            destination: WEBSITE
          }
          policy {
            cookies_allowed: YES
            cookies_store: "user"
            setting:
              "This feature cannot be disabled by settings, but it is only "
              "triggered by user request."
            policy_exception_justification: "Not implemented."
          })");
    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url = original_url_;
    resource_request->load_flags = net::LOAD_DISABLE_CACHE;
    simple_loader_ = network::SimpleURLLoader::Create(
        std::move(resource_request), traffic_annotation);
    network::mojom::URLLoaderFactory* loader_factory =
        g_browser_process->system_network_context_manager()
            ->GetURLLoaderFactory();
    simple_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        loader_factory,
        base::BindOnce(&WallpaperFetcher::OnSimpleLoaderComplete,
                       base::Unretained(this)));
  }

 private:
  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body) {
    std::string response;
    bool success = false;
    if (response_body) {
      response = std::move(*response_body);
      success = true;
    } else if (simple_loader_->ResponseInfo() &&
               simple_loader_->ResponseInfo()->headers) {
      int response_code =
          simple_loader_->ResponseInfo()->headers->response_code();
      response = base::StringPrintf(
          "Downloading wallpaper %s failed. The response code is %d.",
          original_url_.ExtractFileName().c_str(), response_code);
    }

    simple_loader_.reset();
    std::move(callback_).Run(success, response);
  }

  void CancelPreviousFetch() {
    if (simple_loader_.get()) {
      std::move(callback_).Run(false, kCancelWallpaperMessage);
      simple_loader_.reset();
    }
  }

  GURL original_url_;
  std::unique_ptr<network::SimpleURLLoader> simple_loader_;
  FetchCallback callback_;
};

const char WallpaperFetcher::kCancelWallpaperMessage[] =
    "Set wallpaper was canceled.";

base::LazyInstance<WallpaperFetcher>::DestructorAtExit g_wallpaper_fetcher =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

WallpaperSetWallpaperFunction::WallpaperSetWallpaperFunction() {}

WallpaperSetWallpaperFunction::~WallpaperSetWallpaperFunction() {}

ExtensionFunction::ResponseAction WallpaperSetWallpaperFunction::Run() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  params_ = set_wallpaper::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params_);

  if (params_->details.data) {
    SetWallpaperOnAsh();
    return RespondLater();
  }

  if (!params_->details.url) {
    return RespondNow(Error("Either url or data field is required."));
  }

  GURL wallpaper_url(*params_->details.url);
  if (!wallpaper_url.is_valid()) {
    return RespondNow(Error("URL is invalid."));
  }

  g_wallpaper_fetcher.Get().FetchWallpaper(
      wallpaper_url,
      base::BindOnce(&WallpaperSetWallpaperFunction::OnWallpaperFetched, this));
  // FetchWallpaper() responds asynchronously.
  return RespondLater();
}

void WallpaperSetWallpaperFunction::OnWallpaperFetched(
    bool success,
    const std::string& response) {
  if (success) {
    params_->details.data.emplace(response.begin(), response.end());
    SetWallpaperOnAsh();
  } else {
    Respond(Error(response));
  }
}

void WallpaperSetWallpaperFunction::OnWallpaperSetOnAsh(
    const crosapi::mojom::SetWallpaperResultPtr result) {
  if (result->is_thumbnail_data()) {
    Respond(params_->details.thumbnail
                ? WithArguments(Value(std::move(result->get_thumbnail_data())))
                : NoArguments());
  } else {
    Respond(Error(result->get_error_message()));
  }
}

void WallpaperSetWallpaperFunction::SetWallpaperOnAsh() {
  const extensions::Extension* ext = extension();
  std::string extension_id;
  std::string extension_name;
  if (ext) {
    extension_id = ext->id();
    extension_name = ext->name();
  }

  crosapi::mojom::WallpaperSettingsPtr settings =
      crosapi::mojom::WallpaperSettings::New();
  settings->data = *params_->details.data;
  settings->layout = GetMojoLayoutEnum(params_->details.layout);
  settings->filename = params_->details.filename;

  auto* wallpaper_api = GetWallpaperApi();
  if (!wallpaper_api) {
    Respond(Error("Unsupported ChromeOS version."));
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto ash_version = chromeos::LacrosService::Get()
                         ->GetInterfaceVersion<crosapi::mojom::Wallpaper>();
  if (ash_version <
      static_cast<int>(crosapi::mojom::Wallpaper::kSetWallpaperMinVersion)) {
    Respond(Error("Unsupported ChromeOS version."));
    return;
  }
  wallpaper_api->SetWallpaper(
      std::move(settings), extension_id, extension_name,
      base::BindOnce(&WallpaperSetWallpaperFunction::OnWallpaperSetOnAsh,
                     this));
#else
  // Without lacros, there is never a version mismatch between this file and
  // wallpaper_ash.
  wallpaper_api->SetWallpaper(
      std::move(settings), extension_id, extension_name,
      base::BindOnce(&WallpaperSetWallpaperFunction::OnWallpaperSetOnAsh,
                     this));
#endif
}
