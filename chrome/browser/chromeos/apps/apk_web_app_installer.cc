// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/apps/apk_web_app_installer.h"

#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "content/public/browser/browser_thread.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {
constexpr int64_t kInvalidColor =
    static_cast<int64_t>(std::numeric_limits<int32_t>::max()) + 1;
}

namespace chromeos {

// static
void ApkWebAppInstaller::Install(Profile* profile,
                                 arc::mojom::WebAppInfoPtr web_app_info,
                                 const std::vector<uint8_t>& icon_png_data,
                                 InstallFinishCallback callback,
                                 base::WeakPtr<Owner> weak_owner) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(weak_owner.get());

  // If |weak_owner| is invalidated, installation will be stopped.
  // ApkWebAppInstaller owns itself and deletes itself when finished in
  // CompleteInstallation().
  auto* installer =
      new ApkWebAppInstaller(profile, std::move(callback), weak_owner);
  installer->Start(std::move(web_app_info), icon_png_data);
}

ApkWebAppInstaller::ApkWebAppInstaller(Profile* profile,
                                       InstallFinishCallback callback,
                                       base::WeakPtr<Owner> weak_owner)
    : profile_(profile),
      callback_(std::move(callback)),
      weak_owner_(weak_owner) {}

ApkWebAppInstaller::~ApkWebAppInstaller() = default;

void ApkWebAppInstaller::Start(arc::mojom::WebAppInfoPtr web_app_info,
                               const std::vector<uint8_t>& icon_png_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!weak_owner_.get()) {
    CompleteInstallation(web_app::AppId(),
                         web_app::InstallResultCode::kFailedUnknownReason);
    return;
  }

  // We can't install without |web_app_info| or |icon_png_data|. They may be
  // null if there was an error generating the data.
  if (web_app_info.is_null() || icon_png_data.empty()) {
    LOG(ERROR) << "Insufficient data to install a web app";
    CompleteInstallation(web_app::AppId(),
                         web_app::InstallResultCode::kFailedUnknownReason);
    return;
  }

  DCHECK(!web_app_info_);
  web_app_info_ = std::make_unique<WebApplicationInfo>();

  web_app_info_->title = base::UTF8ToUTF16(web_app_info->title);

  web_app_info_->app_url = GURL(web_app_info->start_url);
  DCHECK(web_app_info_->app_url.is_valid());

  web_app_info_->scope = GURL(web_app_info->scope_url);
  DCHECK(web_app_info_->scope.is_valid());

  if (web_app_info->theme_color != kInvalidColor) {
    web_app_info_->theme_color =
        static_cast<SkColor>(web_app_info->theme_color);
  }
  web_app_info_->display_mode = blink::mojom::DisplayMode::kStandalone;
  web_app_info_->open_as_window = true;

  // Decode the image in a sandboxed process off the main thread.
  // base::Unretained is safe because this object owns itself.
  data_decoder::DecodeImageIsolated(
      icon_png_data, data_decoder::mojom::ImageCodec::DEFAULT,
      /*shrink_to_fit=*/false, data_decoder::kDefaultMaxSizeInBytes,
      /*desired_image_frame_size=*/gfx::Size(),
      base::BindOnce(&ApkWebAppInstaller::OnImageDecoded,
                     base::Unretained(this)));
}

void ApkWebAppInstaller::CompleteInstallation(const web_app::AppId& id,
                                              web_app::InstallResultCode code) {
  std::move(callback_).Run(id, code);
  delete this;
}

void ApkWebAppInstaller::OnWebAppCreated(const GURL& app_url,
                                         const web_app::AppId& app_id,
                                         web_app::InstallResultCode code) {
  // It is assumed that if |weak_owner_| is gone, |profile_| is gone too. The
  // web app will be automatically cleaned up by provider.
  if (!weak_owner_.get()) {
    CompleteInstallation(web_app::AppId(),
                         web_app::InstallResultCode::kProfileDestroyed);
    return;
  }

  if (code != web_app::InstallResultCode::kSuccessNewInstall) {
    CompleteInstallation(app_id, code);
    return;
  }

  // Otherwise, insert this web app into the extensions ID map so it is not
  // removed automatically. TODO(crbug.com/910008): have a less bad way of doing
  // this.
  web_app::ExternallyInstalledWebAppPrefs(profile_->GetPrefs())
      .Insert(app_url, app_id, web_app::ExternalInstallSource::kArc);
  CompleteInstallation(app_id, code);
}

void ApkWebAppInstaller::OnImageDecoded(const SkBitmap& decoded_image) {
  DCHECK(web_app_info_);

  WebApplicationIconInfo icon_info;
  icon_info.data = decoded_image;
  icon_info.width = decoded_image.width();
  icon_info.height = decoded_image.height();

  web_app_info_->icons.push_back(icon_info);

  if (!weak_owner_.get()) {
    // Assume |profile_| is no longer valid - destroy this object and
    // terminate.
    CompleteInstallation(web_app::AppId(),
                         web_app::InstallResultCode::kProfileDestroyed);
    return;
  }
  DoInstall();
}

void ApkWebAppInstaller::DoInstall() {
  auto* provider = web_app::WebAppProvider::Get(profile_);
  DCHECK(provider);

  GURL app_url = web_app_info_->app_url;

  provider->install_manager().InstallWebAppFromInfo(
      std::move(web_app_info_), web_app::ForInstallableSite::kYes,
      WebappInstallSource::ARC,
      base::BindOnce(&ApkWebAppInstaller::OnWebAppCreated,
                     base::Unretained(this), std::move(app_url)));
}

}  // namespace chromeos
