// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/wallpaper_api.h"

#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/wallpaper_types.h"
#include "base/bind.h"
#include "base/check_op.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/extensions/wallpaper_private_api.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client_impl.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/event_router.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"

using base::Value;
using content::BrowserThread;

using FetchCallback =
    base::OnceCallback<void(bool success, const std::string&)>;

namespace set_wallpaper = extensions::api::wallpaper::SetWallpaper;

namespace {

class WallpaperFetcher {
 public:
  WallpaperFetcher() {}

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
      std::move(callback_).Run(false,
                               wallpaper_api_util::kCancelWallpaperMessage);
      simple_loader_.reset();
    }
  }

  GURL original_url_;
  std::unique_ptr<network::SimpleURLLoader> simple_loader_;
  FetchCallback callback_;
};

base::LazyInstance<WallpaperFetcher>::DestructorAtExit g_wallpaper_fetcher =
    LAZY_INSTANCE_INITIALIZER;

// Gets the |User| for a given |BrowserContext|. The function will only return
// valid objects.
const user_manager::User* GetUserFromBrowserContext(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  DCHECK(profile);
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  DCHECK(user);
  return user;
}

}  // namespace

WallpaperSetWallpaperFunction::WallpaperSetWallpaperFunction() {
}

WallpaperSetWallpaperFunction::~WallpaperSetWallpaperFunction() {
}

ExtensionFunction::ResponseAction WallpaperSetWallpaperFunction::Run() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  params_ = set_wallpaper::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_);

  // Gets account id from the caller, ensuring multiprofile compatibility.
  const user_manager::User* user = GetUserFromBrowserContext(browser_context());
  account_id_ = user->GetAccountId();
  wallpaper_files_id_ =
      WallpaperControllerClientImpl::Get()->GetFilesId(account_id_);

  if (params_->details.data) {
    StartDecode(*params_->details.data);
    // StartDecode() responds asynchronously.
    return RespondLater();
  }

  if (!params_->details.url)
    return RespondNow(Error("Either url or data field is required."));

  GURL wallpaper_url(*params_->details.url);
  if (!wallpaper_url.is_valid())
    return RespondNow(Error("URL is invalid."));

  g_wallpaper_fetcher.Get().FetchWallpaper(
      wallpaper_url,
      base::BindOnce(&WallpaperSetWallpaperFunction::OnWallpaperFetched, this));
  // FetchWallpaper() repsonds asynchronously.
  return RespondLater();
}

void WallpaperSetWallpaperFunction::OnWallpaperDecoded(
    const gfx::ImageSkia& image) {
  ash::WallpaperLayout layout = wallpaper_api_util::GetLayoutEnum(
      extensions::api::wallpaper::ToString(params_->details.layout));
  wallpaper_api_util::RecordCustomWallpaperLayout(layout);

  const std::string file_name =
      base::FilePath(params_->details.filename).BaseName().value();
  WallpaperControllerClientImpl::Get()->SetCustomWallpaper(
      account_id_, wallpaper_files_id_, file_name, layout, image,
      /*preview_mode=*/false);
  unsafe_wallpaper_decoder_ = nullptr;

  // We need to generate thumbnail image anyway to make the current third party
  // wallpaper syncable through different devices.
  image.EnsureRepsForSupportedScales();
  std::vector<uint8_t> thumbnail_data = GenerateThumbnail(
      image, gfx::Size(kWallpaperThumbnailWidth, kWallpaperThumbnailHeight));

  // Inform the native Wallpaper Picker Application that the current wallpaper
  // has been modified by a third party application.
  if (extension()->id() != extension_misc::kWallpaperManagerId) {
    Profile* profile = Profile::FromBrowserContext(browser_context());
    extensions::EventRouter* event_router =
        extensions::EventRouter::Get(profile);

    base::Value event_args(Value::Type::LIST);
    event_args.Append(Value(GenerateThumbnail(image, image.size())));
    event_args.Append(Value(thumbnail_data));
    event_args.Append(
        extensions::api::wallpaper::ToString(params_->details.layout));
    // Setting wallpaper from right click menu in 'Files' app is a feature that
    // was implemented in crbug.com/578935. Since 'Files' app is a built-in v1
    // app in ChromeOS, we should treat it slightly differently with other third
    // party apps: the wallpaper set by the 'Files' app should still be syncable
    // and it should not appear in the wallpaper grid in the Wallpaper Picker.
    // But we should not display the 'wallpaper-set-by-mesage' since it might
    // introduce confusion as shown in crbug.com/599407.
    event_args.Append((extension()->id() == file_manager::kFileManagerAppId)
                          ? base::StringPiece()
                          : extension()->name());
    std::unique_ptr<extensions::Event> event(new extensions::Event(
        extensions::events::WALLPAPER_PRIVATE_ON_WALLPAPER_CHANGED_BY_3RD_PARTY,
        extensions::api::wallpaper_private::OnWallpaperChangedBy3rdParty::
            kEventName,
        base::ListValue::From(std::make_unique<Value>(std::move(event_args)))));
    event_router->DispatchEventToExtension(extension_misc::kWallpaperManagerId,
                                           std::move(event));
  }

  Respond(params_->details.thumbnail
              ? OneArgument(Value(std::move(thumbnail_data)))
              : NoArguments());
}

void WallpaperSetWallpaperFunction::OnWallpaperFetched(
    bool success,
    const std::string& response) {
  if (success) {
    params_->details.data.reset(
        new std::vector<uint8_t>(response.begin(), response.end()));
    StartDecode(*params_->details.data);
    // StartDecode() will Respond later through OnWallpaperDecoded()
  } else {
    Respond(Error(response));
  }
}
