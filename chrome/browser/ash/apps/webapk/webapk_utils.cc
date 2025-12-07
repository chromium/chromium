// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/apps/webapk/webapk_utils.h"

#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/services/app_service/public/cpp/share_target.h"

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

}  // namespace apps
