// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_apps_publisher.h"

#include <utility>

#include "chrome/browser/apps/app_service/media_requests.h"
#include "chrome/browser/lacros/for_which_extension_type.h"
#include "chrome/browser/lacros/lacros_extensions_util.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/app_constants/constants.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"

LacrosAppsPublisher::LacrosAppsPublisher() {}

LacrosAppsPublisher::~LacrosAppsPublisher() = default;

void LacrosAppsPublisher::Initialize() {
  if (!InitializeCrosapi()) {
    return;
  }

  media_dispatcher_.Observe(MediaCaptureDevicesDispatcher::GetInstance()
                                ->GetMediaStreamCaptureIndicator()
                                .get());
}

bool LacrosAppsPublisher::InitializeCrosapi() {
  auto* service = chromeos::LacrosService::Get();
  if (!service) {
    return false;
  }

  // Ash is too old to support the Lacros publisher interface.
  if (service->GetInterfaceVersion<crosapi::mojom::Crosapi>() <
      int{crosapi::mojom::Crosapi::kBindLacrosAppPublisherMinVersion}) {
    return false;
  }

  service->BindPendingReceiverOrRemote<
      mojo::PendingReceiver<crosapi::mojom::AppPublisher>,
      &crosapi::mojom::Crosapi::BindLacrosAppPublisher>(
      publisher_.BindNewPipeAndPassReceiver());
  return true;
}

void LacrosAppsPublisher::PublishCapabilityAccesses(
    std::vector<apps::CapabilityAccessPtr> accesses) {
  publisher_->OnCapabilityAccesses(std::move(accesses));
}

void LacrosAppsPublisher::OnIsCapturingVideoChanged(
    content::WebContents* web_contents,
    bool is_capturing_video) {
  if (!ShouldModifyCapabilityAccess(web_contents)) {
    return;
  }

  auto result = media_requests_.UpdateCameraState(
      app_constants::kChromeAppId, web_contents, is_capturing_video);
  ModifyCapabilityAccess(result.camera, result.microphone);
}

void LacrosAppsPublisher::OnIsCapturingAudioChanged(
    content::WebContents* web_contents,
    bool is_capturing_audio) {
  if (!ShouldModifyCapabilityAccess(web_contents)) {
    return;
  }

  auto result = media_requests_.UpdateMicrophoneState(
      app_constants::kChromeAppId, web_contents, is_capturing_audio);
  ModifyCapabilityAccess(result.camera, result.microphone);
}

bool LacrosAppsPublisher::ShouldModifyCapabilityAccess(
    content::WebContents* web_contents) {
  // The web app publisher is responsible to handle `web_contents` for web
  // apps.
  const webapps::AppId* web_app_id =
      web_app::WebAppTabHelper::GetAppId(web_contents);
  if (web_app_id) {
    return false;
  }

  // The lacros extension app publisher is responsible to handle `web_contents`
  // for chrome apps.
  const auto* extension =
      lacros_extensions_util::MaybeGetExtension(web_contents);
  if (extension && InitForChromeApps().Matches(extension)) {
    return false;
  }

  return true;
}

void LacrosAppsPublisher::ModifyCapabilityAccess(
    std::optional<bool> accessing_camera,
    std::optional<bool> accessing_microphone) {
  if (!accessing_camera.has_value() && !accessing_microphone.has_value()) {
    return;
  }

  std::vector<apps::CapabilityAccessPtr> capability_accesses;
  auto capability_access =
      std::make_unique<apps::CapabilityAccess>(app_constants::kLacrosAppId);
  capability_access->camera = accessing_camera;
  capability_access->microphone = accessing_microphone;
  capability_accesses.push_back(std::move(capability_access));

  PublishCapabilityAccesses(std::move(capability_accesses));
}
