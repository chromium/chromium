// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/webapk/webapk_utils.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/location.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/services/app_service/public/cpp/share_target.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/smhasher/src/MurmurHash2.h"
#include "url/gurl.h"

// TODO(crbug.com/40199484): Consolidate logic with apps::WebApkInstallTask.

#if BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {

const web_app::SquareSizePx kMinimumIconSize = 64;

// The seed to use when taking the murmur2 hash of the icon.
const uint64_t kMurmur2HashSeed = 0;

crosapi::mojom::WebApkCreationParamsPtr AddIconDataAndSerializeProto(
    const GURL& manifest_url,
    std::unique_ptr<webapk::WebAppManifest> webapk_manifest,
    std::vector<uint8_t> icon_data) {
  base::AssertLongCPUWorkAllowed();
  DCHECK_EQ(webapk_manifest->icons_size(), 1);

  webapk::Image* icon = webapk_manifest->mutable_icons(0);
  icon->set_image_data(icon_data.data(), icon_data.size());

  uint64_t icon_hash =
      MurmurHash64A(icon_data.data(), icon_data.size(), kMurmur2HashSeed);
  icon->set_hash(base::NumberToString(icon_hash));

  std::vector<uint8_t> serialized_proto(webapk_manifest->ByteSize());
  webapk_manifest->SerializeToArray(serialized_proto.data(),
                                    webapk_manifest->ByteSize());

  return crosapi::mojom::WebApkCreationParams::New(manifest_url.spec(),
                                                   std::move(serialized_proto));
}

void OnLoadedIcon(apps::GetWebApkCreationParamsCallback callback,
                  const GURL& manifest_url,
                  std::unique_ptr<webapk::WebAppManifest> webapk_manifest,
                  web_app::IconPurpose purpose,
                  std::vector<uint8_t> data) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce(AddIconDataAndSerializeProto, manifest_url,
                     std::move(webapk_manifest), std::move(data)),
      std::move(callback));
}

}  // namespace

#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace apps {

void PopulateWebApkManifest(Profile* profile,
                            const std::string& app_id,
                            webapk::WebAppManifest* web_app_manifest) {
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
  auto& registrar = provider->registrar_unsafe();

  // TODO(crbug.com/40199484): Call WebAppRegistrar::GetAppById(const AppId&
  // app_id) instead of performing repeated app_id lookups.

  web_app_manifest->set_short_name(registrar.GetAppShortName(app_id));
  web_app_manifest->set_start_url(registrar.GetAppStartUrl(app_id).spec());
  web_app_manifest->add_scopes(registrar.GetAppScope(app_id).spec());

  // We currently don't set name, orientation, display_mode, theme_color,
  // background_color, shortcuts.

  auto* share_target = registrar.GetAppShareTarget(app_id);
  webapk::ShareTarget* proto_share_target =
      web_app_manifest->add_share_targets();
  proto_share_target->set_action(share_target->action.spec());
  proto_share_target->set_method(
      apps::ShareTarget::MethodToString(share_target->method));
  proto_share_target->set_enctype(
      apps::ShareTarget::EnctypeToString(share_target->enctype));

  webapk::ShareTargetParams* proto_params =
      proto_share_target->mutable_params();
  if (!share_target->params.title.empty()) {
    proto_params->set_title(share_target->params.title);
  }
  if (!share_target->params.text.empty()) {
    proto_params->set_text(share_target->params.text);
  }
  if (!share_target->params.url.empty()) {
    proto_params->set_url(share_target->params.url);
  }

  for (const auto& file : share_target->params.files) {
    webapk::ShareTargetParamsFile* proto_file = proto_params->add_files();
    proto_file->set_name(file.name);
    for (const auto& accept_type : file.accept) {
      proto_file->add_accept(accept_type);
    }
  }
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)

void GetWebApkCreationParams(Profile* profile,
                             const std::string& app_id,
                             GetWebApkCreationParamsCallback callback) {
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
  if (!provider) {
    DVLOG(1) << "WebAppProvider is not available.";
    std::move(callback).Run({});
    return;
  }

  auto& registrar = provider->registrar_unsafe();

  // TODO(crbug.com/40199484): Call WebAppRegistrar::GetAppById(const AppId&
  // app_id) instead of performing repeated app_id lookups.

  // Installation & share target are already checked in WebApkManager, check
  // again in case anything changed while the install request was queued.
  // Manifest URL is always set for apps installed or updated in recent
  // versions, but might be missing for older apps.
  if (!registrar.IsInstalled(app_id) || !registrar.GetAppShareTarget(app_id) ||
      registrar.GetAppManifestUrl(app_id).is_empty()) {
    DVLOG(1) << "App is not installed, has no share target or is invalid.";
    std::move(callback).Run({});
    return;
  }

  auto webapk_manifest = std::make_unique<webapk::WebAppManifest>();

  auto& icon_manager = provider->icon_manager();
  std::optional<web_app::WebAppIconManager::IconSizeAndPurpose>
      icon_size_and_purpose = icon_manager.FindIconMatchBigger(
          app_id, {web_app::IconPurpose::MASKABLE, web_app::IconPurpose::ANY},
          kMinimumIconSize);

  if (!icon_size_and_purpose) {
    LOG(ERROR) << "Could not find suitable icon";
    std::move(callback).Run({});
    return;
  }

  // We need to send a URL for the icon, but it's possible the local image we're
  // sending has been resized and so doesn't exactly match any of the images in
  // the manifest. Since we can't be perfect, it's okay to be roughly correct
  // and just send any URL of the correct purpose.
  const auto& manifest_icons = registrar.GetAppIconInfos(app_id);
  auto it = base::ranges::find_if(
      manifest_icons, [&icon_size_and_purpose](const apps::IconInfo& info) {
        return info.purpose == web_app::ManifestPurposeToIconInfoPurpose(
                                   icon_size_and_purpose->purpose);
      });

  if (it == manifest_icons.end()) {
    LOG(ERROR) << "Could not find suitable icon";
    std::move(callback).Run({});
    return;
  }
  std::string icon_url = it->url.spec();

  PopulateWebApkManifest(profile, app_id, webapk_manifest.get());

  webapk::Image* image = webapk_manifest->add_icons();
  image->set_src(std::move(icon_url));
  image->add_purposes(icon_size_and_purpose->purpose ==
                              web_app::IconPurpose::MASKABLE
                          ? webapk::Image::MASKABLE
                          : webapk::Image::ANY);
  image->add_usages(webapk::Image::PRIMARY_ICON);

  icon_manager.ReadSmallestCompressedIcon(
      app_id, {icon_size_and_purpose->purpose}, icon_size_and_purpose->size_px,
      base::BindOnce(&OnLoadedIcon, std::move(callback),
                     registrar.GetAppManifestUrl(app_id),
                     std::move(webapk_manifest)));
}

#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace apps
