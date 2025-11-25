// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/web_app/kiosk_web_app_data.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/kiosk_app_data_base.h"
#include "chrome/browser/ash/app_mode/kiosk_app_data_delegate.h"
#include "chrome/browser/ash/app_mode/web_app/kiosk_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_thread.h"
#include "ipc/constants.mojom.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/fetch_api.mojom-data-view.h"
#include "skia/ext/image_operations.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

namespace {

const char kKeyLaunchUrl[] = "launch_url";
const char kKeyLastIconUrl[] = "last_icon_url";

// Resizes image into other size on blocking I/O thread.
SkBitmap ResizeImageBlocking(const SkBitmap& image, int target_size) {
  return skia::ImageOperations::Resize(
      image, skia::ImageOperations::RESIZE_BEST, target_size, target_size);
}

}  // namespace

class KioskWebAppData::IconFetcher {
 public:
  using ResultCallback = base::OnceCallback<void(SkBitmap)>;

  explicit IconFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory)
      : shared_url_loader_factory_(std::move(shared_url_loader_factory)) {}
  IconFetcher(const IconFetcher&) = delete;
  IconFetcher& operator=(const IconFetcher&) = delete;
  ~IconFetcher() = default;

  // `callback` will be called unless `this` is deleted.
  // When download or decode fails, `callback` is invoked with a null SkBitmap.
  void Start(const GURL& icon_url, ResultCallback callback) {
    CHECK(callback);

    // `Start` must not be called multiple times.
    CHECK(!started_);
    started_ = true;

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
          internal {
            contacts {
              owners: "//chromeos/components/kiosk/OWNERS"
            }
          }
          data: "None"
          user_data {
            type: NONE
          }
          destination: WEBSITE
          last_reviewed: "2025-05-21"
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled in settings."
          policy_exception_justification:
            "No content is being uploaded or saved; this request merely "
            "downloads a publicly available PNG file."
        })");
    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url = icon_url;
    resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
    simple_loader_ = network::SimpleURLLoader::Create(
        std::move(resource_request), traffic_annotation);

    simple_loader_->SetRetryOptions(
        /* max_retries=*/3,
        network::SimpleURLLoader::RETRY_ON_5XX |
            network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);

    // Assuming maximum image size is 256x256.
    constexpr int kMaxIconFileSize = (2 * KioskWebAppData::kIconSize) *
                                         (2 * KioskWebAppData::kIconSize) * 4 +
                                     1000;

    simple_loader_->DownloadToString(
        shared_url_loader_factory_.get(),
        base::BindOnce(&IconFetcher::OnDownloadCompleted,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
        kMaxIconFileSize);
  }

 private:
  void OnDownloadCompleted(ResultCallback callback,
                           std::optional<std::string> response_body) {
    // Now simple_loader_ can be released safely.
    simple_loader_.reset();

    if (!response_body) {
      LOG(ERROR) << "Could not download icon url for kiosk app.";
      std::move(callback).Run(SkBitmap());
      return;
    }

    // Start decoding. `OnImageDecoded` will be called when decoding is
    // complete. It's called with a null SkBitmap on error.
    data_decoder::DecodeImageIsolated(
        base::as_byte_span(*response_body),
        data_decoder::mojom::ImageCodec::kDefault,
        /*shrink_to_fit=*/false,
        static_cast<int64_t>(IPC::mojom::kChannelMaximumMessageSize),
        /*desired_image_frame_size=*/gfx::Size(),
        base::BindOnce(&IconFetcher::OnImageDecoded,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnImageDecoded(ResultCallback callback, const SkBitmap& bitmap) {
    if (bitmap.isNull()) {
      LOG(ERROR) << "Icon image url does not contain a valid image.";
      std::move(callback).Run(SkBitmap());
      return;
    }

    // Icons have to be square shaped.
    if (bitmap.width() != bitmap.height() || bitmap.empty()) {
      LOG(ERROR) << "Received kiosk icon of invalid shape.";
      std::move(callback).Run(SkBitmap());
      return;
    }

    if (bitmap.width() == KioskWebAppData::kIconSize) {
      std::move(callback).Run(bitmap);
    } else {
      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE,
          {base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
          base::BindOnce(&ResizeImageBlocking, bitmap,
                         KioskWebAppData::kIconSize),
          base::BindOnce(&IconFetcher::OnImageResized,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    }
  }

  void OnImageResized(ResultCallback callback, SkBitmap bitmap) {
    std::move(callback).Run(bitmap);
  }

  const scoped_refptr<network::SharedURLLoaderFactory>
      shared_url_loader_factory_;

  bool started_ = false;
  std::unique_ptr<network::SimpleURLLoader> simple_loader_;
  base::WeakPtrFactory<IconFetcher> weak_ptr_factory_{this};
};

KioskWebAppData::KioskWebAppData(
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
    KioskAppDataDelegate& delegate,
    const std::string& app_id,
    const AccountId& account_id,
    const GURL url,
    const std::string& title,
    const GURL icon_url)
    : KioskAppDataBase(local_state,
                       KioskWebAppManager::kWebKioskDictionaryName,
                       app_id,
                       account_id),
      shared_url_loader_factory_(std::move(shared_url_loader_factory)),
      delegate_(delegate),
      status_(Status::kInit),
      install_url_(url),
      icon_url_(icon_url) {
  name_ = title.empty() ? install_url_.spec() : title;
}

KioskWebAppData::~KioskWebAppData() = default;

bool KioskWebAppData::LoadFromCache() {
  const base::Value::Dict& dict = local_state_->GetDict(dictionary_name());

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

void KioskWebAppData::LoadIcon() {
  if (!icon_.isNull()) {
    return;
  }

  // Decode the icon if one is already cached.
  if (status_ != Status::kInit) {
    DecodeIcon(base::BindOnce(&KioskWebAppData::OnIconLoadDone,
                              weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  if (!icon_url_.is_valid()) {
    return;
  }

  status_ = Status::kLoading;

  DCHECK(!icon_fetcher_);
  icon_fetcher_ = std::make_unique<IconFetcher>(shared_url_loader_factory_);
  icon_fetcher_->Start(icon_url_,
                       base::BindOnce(&KioskWebAppData::OnDidDownloadIcon,
                                      weak_ptr_factory_.GetWeakPtr()));
}

GURL KioskWebAppData::GetLaunchableUrl() const {
  return status() == KioskWebAppData::Status::kInstalled ? launch_url()
                                                         : install_url();
}

void KioskWebAppData::UpdateFromWebAppInfo(
    const web_app::WebAppInstallInfo& app_info) {
  UpdateAppInfo(base::UTF16ToUTF8(app_info.title.value()), app_info.start_url(),
                app_info.GetIconBitmapsForSecureSurfaces().bitmaps);
}

void KioskWebAppData::UpdateAppInfo(const std::string& title,
                                    const GURL& start_url,
                                    const web_app::SizeToBitmap& icon_bitmaps) {
  name_ = title;

  auto it = icon_bitmaps.find(kIconSize);
  if (it != icon_bitmaps.end()) {
    const SkBitmap& bitmap = it->second;
    icon_ = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
    icon_.MakeThreadSafe();
    SaveIcon(bitmap, delegate_->GetKioskAppIconCacheDir());
  }

  ScopedDictPrefUpdate dict_update(&local_state_.get(), dictionary_name());
  SaveToDictionary(dict_update);

  launch_url_ = start_url;
  dict_update->FindDict(KioskAppDataBase::kKeyApps)
      ->FindDict(app_id())
      ->Set(kKeyLaunchUrl, launch_url_.spec());

  SetStatus(Status::kInstalled);
}

void KioskWebAppData::SetOnLoadedCallbackForTesting(
    base::OnceClosure callback) {
  on_loaded_closure_for_testing_ = std::move(callback);
}

void KioskWebAppData::SetStatus(Status status, bool notify) {
  status_ = status;

  if (status_ == Status::kLoaded && on_loaded_closure_for_testing_) {
    std::move(on_loaded_closure_for_testing_).Run();
  }

  if (notify) {
    delegate_->OnKioskAppDataChanged(app_id());
  }
}

bool KioskWebAppData::LoadLaunchUrlFromDictionary(
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

GURL KioskWebAppData::GetLastIconUrl(const base::Value::Dict& dict) const {
  // All the previous keys should be present since this function is executed
  // after LoadFromDictionary().
  const std::string* icon_url_string = dict.FindDict(KioskAppDataBase::kKeyApps)
                                           ->FindDict(app_id())
                                           ->FindString(kKeyLastIconUrl);

  return icon_url_string ? GURL(*icon_url_string) : GURL();
}

void KioskWebAppData::OnDidDownloadIcon(SkBitmap icon) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (icon.isNull()) {
    // NOTE: Probably we should do something on error cases.
    return;
  }

  CHECK_EQ(icon.width(), KioskWebAppData::kIconSize);

  icon_fetcher_.reset();

  if (status_ == Status::kInstalled) {
    return;
  }

  SaveIcon(icon, delegate_->GetKioskAppIconCacheDir());

  ScopedDictPrefUpdate dict_update(&local_state_.get(), dictionary_name());
  SaveIconToDictionary(dict_update);

  dict_update->FindDict(KioskAppDataBase::kKeyApps)
      ->FindDict(app_id())
      ->Set(kKeyLastIconUrl, icon_url_.spec());

  SetStatus(Status::kLoaded);
}

void KioskWebAppData::OnIconLoadDone(std::optional<gfx::ImageSkia> icon) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

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
