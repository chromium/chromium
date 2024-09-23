// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/apps/apk_web_app_installer.h"

#include <limits>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/web_app_service_ash.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/browser_thread.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {
constexpr int64_t kInvalidColor =
    static_cast<int64_t>(std::numeric_limits<int32_t>::max()) + 1;
}

namespace ash {

// static
void ApkWebAppInstaller::Install(Profile* profile,
                                 const std::string& package_name,
                                 arc::mojom::WebAppInfoPtr web_app_info,
                                 arc::mojom::RawIconPngDataPtr icon,
                                 InstallFinishCallback callback,
                                 base::WeakPtr<Owner> weak_owner) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(weak_owner.get());

  // If |weak_owner| is invalidated, installation will be stopped.
  // ApkWebAppInstaller owns itself and deletes itself when finished in
  // CompleteInstallation().
  auto* installer =
      new ApkWebAppInstaller(profile, std::move(callback), weak_owner);
  installer->Start(package_name, std::move(web_app_info), std::move(icon));
}

ApkWebAppInstaller::ApkWebAppInstaller(Profile* profile,
                                       InstallFinishCallback callback,
                                       base::WeakPtr<Owner> weak_owner)
    : profile_(profile),
      is_web_only_twa_(false),
      sha256_fingerprint_(std::nullopt),
      callback_(std::move(callback)),
      weak_owner_(weak_owner) {}

ApkWebAppInstaller::~ApkWebAppInstaller() = default;

void ApkWebAppInstaller::Start(const std::string& package_name,
                               arc::mojom::WebAppInfoPtr arc_web_app_info,
                               arc::mojom::RawIconPngDataPtr icon) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!weak_owner_.get()) {
    CompleteInstallation(webapps::AppId(),
                         webapps::InstallResultCode::kApkWebAppInstallFailed);
    return;
  }

  // We can't install without |arc_web_app_info| or |icon_png_data|. They
  // may be null if there was an error generating the data.
  if (arc_web_app_info.is_null() || !icon || !icon->icon_png_data ||
      !icon->icon_png_data.has_value() || icon->icon_png_data->empty()) {
    LOG(ERROR) << "Insufficient data to install a web app";
    CompleteInstallation(webapps::AppId(),
                         webapps::InstallResultCode::kApkWebAppInstallFailed);
    return;
  }

  DCHECK(!web_app_install_info_);
  auto start_url = GURL(arc_web_app_info->start_url);
  // TODO(b:340994232): ARC-installed web apps should pass through a manifest ID
  // and use it here instead of assuming it is not set and generating it from
  // the start URL.
  webapps::ManifestId manifest_id =
      web_app::GenerateManifestIdFromStartUrlOnly(start_url);
  web_app_install_info_ =
      std::make_unique<web_app::WebAppInstallInfo>(manifest_id, start_url);

  web_app_install_info_->title = base::UTF8ToUTF16(arc_web_app_info->title);

  web_app_install_info_->scope = GURL(arc_web_app_info->scope_url);
  DCHECK(web_app_install_info_->scope.is_valid());

  web_app_install_info_->additional_policy_ids.push_back(package_name);

  // The install_url and the start_url seem to be same in this case.
  // This is because inside OnWebAppCreated(), the start_url is
  // passed to the external prefs to be stored as the install_url.
  web_app_install_info_->install_url = GURL(arc_web_app_info->start_url);
  DCHECK(web_app_install_info_->install_url.is_valid());

  if (arc_web_app_info->theme_color != kInvalidColor) {
    web_app_install_info_->theme_color = SkColorSetA(
        static_cast<SkColor>(arc_web_app_info->theme_color), SK_AlphaOPAQUE);
  }
  web_app_install_info_->display_mode = blink::mojom::DisplayMode::kStandalone;
  web_app_install_info_->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;

  is_web_only_twa_ = arc_web_app_info->is_web_only_twa;
  sha256_fingerprint_ = arc_web_app_info->certificate_sha256_fingerprint;

  // Decode the image in a sandboxed process off the main thread.
  // base::Unretained is safe because this object owns itself.
  data_decoder::DecodeImageIsolated(
      std::move(icon->icon_png_data.value()),
      data_decoder::mojom::ImageCodec::kDefault,
      /*shrink_to_fit=*/false, data_decoder::kDefaultMaxSizeInBytes,
      /*desired_image_frame_size=*/gfx::Size(),
      base::BindOnce(&ApkWebAppInstaller::OnImageDecoded,
                     base::Unretained(this)));
}

void ApkWebAppInstaller::CompleteInstallation(const webapps::AppId& id,
                                              webapps::InstallResultCode code) {
  std::move(callback_).Run(id, is_web_only_twa_, sha256_fingerprint_, code);
  delete this;
}

void ApkWebAppInstaller::OnWebAppCreated(const GURL& start_url,
                                         const webapps::AppId& app_id,
                                         webapps::InstallResultCode code) {
  // It is assumed that if |weak_owner_| is gone, |profile_| is gone too. The
  // web app will be automatically cleaned up by provider.
  if (!weak_owner_.get()) {
    CompleteInstallation(
        webapps::AppId(),
        webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown);
    return;
  }

  if (code != webapps::InstallResultCode::kSuccessNewInstall) {
    CompleteInstallation(app_id, code);
    return;
  }

  CompleteInstallation(app_id, code);
}

void ApkWebAppInstaller::OnImageDecoded(const SkBitmap& decoded_image) {
  DCHECK(web_app_install_info_);

  if (decoded_image.width() == decoded_image.height())
    web_app_install_info_->icon_bitmaps.any[decoded_image.width()] =
        decoded_image;

  if (!weak_owner_.get()) {
    // Assume |profile_| is no longer valid - destroy this object and
    // terminate.
    CompleteInstallation(
        webapps::AppId(),
        webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown);
    return;
  }
  DoInstall();
}

void ApkWebAppInstaller::DoInstall() {
  if (web_app::IsWebAppsCrosapiEnabled()) {
    GURL start_url = web_app_install_info_->start_url();

    std::unique_ptr<web_app::WebAppInstallInfo> web_app_install_info =
        std::move(web_app_install_info_);
    auto arc_install_info = crosapi::mojom::ArcWebAppInstallInfo::New();
    arc_install_info->title = std::move(web_app_install_info->title);
    arc_install_info->start_url = std::move(web_app_install_info->start_url());
    arc_install_info->scope = std::move(web_app_install_info->scope);
    arc_install_info->theme_color = web_app_install_info->theme_color;
    arc_install_info->additional_policy_ids =
        std::move(web_app_install_info->additional_policy_ids);
    // Take the first icon (there should only be one).
    if (web_app_install_info->icon_bitmaps.any.size() > 0) {
      auto& [sizePx, bitmap] =
          *std::begin(web_app_install_info->icon_bitmaps.any);
      arc_install_info->icon = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
    }

    crosapi::mojom::WebAppProviderBridge* web_app_provider_bridge =
        crosapi::CrosapiManager::Get()
            ->crosapi_ash()
            ->web_app_service_ash()
            ->GetWebAppProviderBridge();
    if (!web_app_provider_bridge) {
      CompleteInstallation(webapps::AppId(),
                           webapps::InstallResultCode::kWebAppProviderNotReady);
      return;
    }
    web_app_provider_bridge->WebAppInstalledInArc(
        std::move(arc_install_info),
        base::BindOnce(&ApkWebAppInstaller::OnWebAppCreated,
                       base::Unretained(this), std::move(start_url)));
  } else {
    auto* provider = web_app::WebAppProvider::GetForWebApps(profile_);
    DCHECK(provider);
    // Doesn't overwrite already existing web app with manifest fields from the
    // apk.
    GURL start_url = web_app_install_info_->start_url();
    provider->scheduler().InstallFromInfoWithParams(
        std::move(web_app_install_info_),
        /*overwrite_existing_manifest_fields=*/false,
        webapps::WebappInstallSource::ARC,
        base::BindOnce(&ApkWebAppInstaller::OnWebAppCreated,
                       base::Unretained(this), std::move(start_url)),
        web_app::WebAppInstallParams());
  }
}

}  // namespace ash
