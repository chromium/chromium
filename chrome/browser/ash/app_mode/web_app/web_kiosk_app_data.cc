// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_data.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/kiosk_app_data_base.h"
#include "chrome/browser/ash/app_mode/kiosk_app_data_delegate.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/image_decoder/image_decoder.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_thread.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "skia/ext/image_operations.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

constexpr int kWebKioskIconSize = 128;  // size of the icon in px.

namespace {
// Maximum image size is 256x256..
constexpr int kMaxIconFileSize =
    (2 * kWebKioskIconSize) * (2 * kWebKioskIconSize) * 4 + 1000;

const char kKeyLaunchUrl[] = "launch_url";
const char kKeyLastIconUrl[] = "last_icon_url";

// Resizes image into other size on blocking I/O thread.
SkBitmap ResizeImageBlocking(const SkBitmap& image, int target_size) {
  return skia::ImageOperations::Resize(
      image, skia::ImageOperations::RESIZE_BEST, target_size, target_size);
}

}  // namespace

class WebKioskAppData::IconFetcher : public ImageDecoder::ImageRequest {
 public:
  IconFetcher(const base::WeakPtr<WebKioskAppData>& client,
              const GURL& icon_url)
      : client_(client), icon_url_(icon_url) {}

  void Start() {
    net::NetworkTrafficAnnotationTag traffic_annotation =
        net::DefineNetworkTrafficAnnotation("kiosk_app_icon", R"(
        semantics {
          sender: "Kiosk App Icon Downloader"
          description:
            "The actual meta-data of the kiosk apps that is used to "
            "display the application info can only be obtained upon "
            "the installation of the app itself. Before this happens, "
            "we are using default placeholder icon for the app. To "
            "overcome this issue, the URL with the icon file is being "
            "sent from the device management server. Chromium will "
            "download the image located at this url."
          trigger:
            "User clicks on the menu button with the list of kiosk apps"
          data: "None"
          destination: WEBSITE
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled in settings."
          policy_exception_justification:
            "No content is being uploaded or saved; this request merely "
            "downloads a publicly available PNG file."
        })");
    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url = icon_url_;
    simple_loader_ = network::SimpleURLLoader::Create(
        std::move(resource_request), traffic_annotation);

    simple_loader_->SetRetryOptions(
        /* max_retries=*/3,
        network::SimpleURLLoader::RETRY_ON_5XX |
            network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);

    SystemNetworkContextManager* system_network_context_manager =
        g_browser_process->system_network_context_manager();
    network::mojom::URLLoaderFactory* loader_factory =
        system_network_context_manager->GetURLLoaderFactory();

    simple_loader_->DownloadToString(
        loader_factory,
        base::BindOnce(
            [](base::WeakPtr<WebKioskAppData> client,
               std::unique_ptr<std::string> response_body) {
              if (!client) {
                return;
              }
              client->icon_fetcher_->OnSimpleLoaderComplete(
                  std::move(response_body));
            },
            client_),
        kMaxIconFileSize);
  }

  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body) {
    if (!response_body) {
      LOG(ERROR) << "Could not download icon url for kiosk app.";
      return;
    }
    // Call start to begin decoding.  The ImageDecoder will call OnImageDecoded
    // with the data when it is done.
    ImageDecoder::Start(this, std::move(*response_body));
  }

 private:
  // ImageDecoder::ImageRequest:
  void OnImageDecoded(const SkBitmap& decoded_image) override {
    if (!client_) {
      return;
    }

    // Icons have to be square shaped.
    if (decoded_image.width() != decoded_image.height()) {
      LOG(ERROR) << "Received kiosk icon of invalid shape.";
      return;
    }

    int size = decoded_image.width();
    if (size == kWebKioskIconSize) {
      client_->OnDidDownloadIcon(decoded_image);
      return;
    }

    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::TaskPriority::USER_VISIBLE,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(ResizeImageBlocking, decoded_image, kWebKioskIconSize),
        base::BindOnce(&WebKioskAppData::OnDidDownloadIcon, client_));
  }

  void OnDecodeImageFailed() override {
    // Do nothing.
    LOG(ERROR) << "Could not download icon url for kiosk app.";
  }

  base::WeakPtr<WebKioskAppData> client_;
  const GURL icon_url_;
  std::unique_ptr<network::SimpleURLLoader> simple_loader_;
};

WebKioskAppData::WebKioskAppData(KioskAppDataDelegate* delegate,
                                 const std::string& app_id,
                                 const AccountId& account_id,
                                 const GURL url,
                                 const std::string& title,
                                 const GURL icon_url)
    : KioskAppDataBase(WebKioskAppManager::kWebKioskDictionaryName,
                       app_id,
                       account_id),
      delegate_(delegate),
      status_(Status::kInit),
      install_url_(url),
      icon_url_(icon_url) {
  name_ = title.empty() ? install_url_.spec() : title;
}

WebKioskAppData::~WebKioskAppData() = default;

bool WebKioskAppData::LoadFromCache() {
  PrefService* local_state = g_browser_process->local_state();
  const base::Value::Dict& dict = local_state->GetDict(dictionary_name());

  if (!LoadFromDictionary(dict)) {
    return false;
  }

  if (LoadLaunchUrlFromDictionary(dict)) {
    SetStatus(Status::kInstalled);
    return true;
  }

  // If the icon was previously downloaded using a different url and the app has
  // not been installed earlier, do not use that icon.
  if (GetLastIconUrl(dict) != icon_url_) {
    return false;
  }

  // Wait while icon is loaded.
  if (status_ == Status::kInit) {
    SetStatus(Status::kLoading);
  }
  return true;
}

void WebKioskAppData::LoadIcon() {
  if (!icon_.isNull()) {
    return;
  }

  // Decode the icon if one is already cached.
  if (status_ != Status::kInit) {
    DecodeIcon(base::BindOnce(&WebKioskAppData::OnIconLoadDone,
                              weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  if (!icon_url_.is_valid()) {
    return;
  }

  DCHECK(!icon_fetcher_);

  status_ = Status::kLoading;

  icon_fetcher_ = std::make_unique<WebKioskAppData::IconFetcher>(
      weak_ptr_factory_.GetWeakPtr(), icon_url_);
  icon_fetcher_->Start();
}

GURL WebKioskAppData::GetLaunchableUrl() const {
  return status() == WebKioskAppData::Status::kInstalled ? launch_url()
                                                         : install_url();
}

void WebKioskAppData::UpdateFromWebAppInfo(
    const web_app::WebAppInstallInfo& app_info) {
  UpdateAppInfo(base::UTF16ToUTF8(app_info.title), app_info.start_url(),
                app_info.icon_bitmaps);
}

void WebKioskAppData::UpdateAppInfo(const std::string& title,
                                    const GURL& start_url,
                                    const web_app::IconBitmaps& icon_bitmaps) {
  name_ = title;

  base::FilePath cache_dir;
  if (delegate_) {
    delegate_->GetKioskAppIconCacheDir(&cache_dir);
  }

  auto it = icon_bitmaps.any.find(kWebKioskIconSize);
  if (it != icon_bitmaps.any.end()) {
    const SkBitmap& bitmap = it->second;
    icon_ = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
    icon_.MakeThreadSafe();
    SaveIcon(bitmap, cache_dir);
  }

  PrefService* local_state = g_browser_process->local_state();
  ScopedDictPrefUpdate dict_update(local_state, dictionary_name());
  SaveToDictionary(dict_update);

  launch_url_ = start_url;
  dict_update->FindDict(KioskAppDataBase::kKeyApps)
      ->FindDict(app_id())
      ->Set(kKeyLaunchUrl, launch_url_.spec());

  SetStatus(Status::kInstalled);
}

void WebKioskAppData::SetOnLoadedCallbackForTesting(
    base::OnceClosure callback) {
  on_loaded_closure_for_testing_ = std::move(callback);
}

void WebKioskAppData::SetStatus(Status status, bool notify) {
  status_ = status;

  if (status_ == Status::kLoaded && on_loaded_closure_for_testing_) {
    std::move(on_loaded_closure_for_testing_).Run();
  }

  if (delegate_ && notify) {
    delegate_->OnKioskAppDataChanged(app_id());
  }
}

bool WebKioskAppData::LoadLaunchUrlFromDictionary(
    const base::Value::Dict& dict) {
  // All the previous keys should be present since this function is executed
  // after LoadFromDictionary().
  const std::string* launch_url_string =
      dict.FindDict(KioskAppDataBase::kKeyApps)
          ->FindDict(app_id())
          ->FindString(kKeyLaunchUrl);

  if (!launch_url_string) {
    return false;
  }

  launch_url_ = GURL(*launch_url_string);
  return true;
}

GURL WebKioskAppData::GetLastIconUrl(const base::Value::Dict& dict) const {
  // All the previous keys should be present since this function is executed
  // after LoadFromDictionary().
  const std::string* icon_url_string = dict.FindDict(KioskAppDataBase::kKeyApps)
                                           ->FindDict(app_id())
                                           ->FindString(kKeyLastIconUrl);

  return icon_url_string ? GURL(*icon_url_string) : GURL();
}

void WebKioskAppData::OnDidDownloadIcon(const SkBitmap& icon) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<IconFetcher> fetcher = std::move(icon_fetcher_);

  if (status_ == Status::kInstalled) {
    return;
  }

  base::FilePath cache_dir;
  if (delegate_) {
    delegate_->GetKioskAppIconCacheDir(&cache_dir);
  }

  SaveIcon(icon, cache_dir);

  PrefService* local_state = g_browser_process->local_state();
  ScopedDictPrefUpdate dict_update(local_state, dictionary_name());
  SaveIconToDictionary(dict_update);

  dict_update->FindDict(KioskAppDataBase::kKeyApps)
      ->FindDict(app_id())
      ->Set(kKeyLastIconUrl, icon_url_.spec());

  SetStatus(Status::kLoaded);
}

void WebKioskAppData::OnIconLoadDone(std::optional<gfx::ImageSkia> icon) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  kiosk_app_icon_loader_.reset();

  if (!icon.has_value()) {
    LOG(ERROR) << "Icon Load Failure";
    SetStatus(Status::kLoaded, /*notify=*/false);
    return;
  }

  icon_ = icon.value();
  if (status_ != Status::kInstalled) {
    SetStatus(Status::kLoaded);
  } else {
    SetStatus(Status::kInstalled);  // To notify menu controller.
  }
}

}  // namespace ash
