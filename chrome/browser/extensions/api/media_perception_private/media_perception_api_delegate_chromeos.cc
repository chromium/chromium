// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/media_perception_private/media_perception_api_delegate_chromeos.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/cros_component_installer_chromeos.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/delegate_to_browser_gpu_service_accelerator_factory.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/video_capture/public/mojom/constants.mojom.h"
#include "services/video_capture/public/mojom/device_factory.mojom.h"
#include "services/video_capture/public/mojom/device_factory_provider.mojom.h"

namespace extensions {

namespace {

constexpr char kLightComponentName[] = "rtanalytics-light";
constexpr char kFullComponentName[] = "rtanalytics-full";

std::string GetComponentNameForComponentType(
    const extensions::api::media_perception_private::ComponentType& type) {
  switch (type) {
    case extensions::api::media_perception_private::COMPONENT_TYPE_LIGHT:
      return kLightComponentName;
    case extensions::api::media_perception_private::COMPONENT_TYPE_FULL:
      return kFullComponentName;
    case extensions::api::media_perception_private::COMPONENT_TYPE_NONE:
      LOG(ERROR) << "No component type requested.";
      return "";
  }
  NOTREACHED() << "Reached component type not in switch.";
  return "";
}

void OnLoadComponent(
    MediaPerceptionAPIDelegate::LoadCrOSComponentCallback load_callback,
    component_updater::CrOSComponentManager::Error error,
    const base::FilePath& mount_point) {
  std::move(load_callback)
      .Run(error == component_updater::CrOSComponentManager::Error::NONE,
           mount_point);
}

}  // namespace

MediaPerceptionAPIDelegateChromeOS::MediaPerceptionAPIDelegateChromeOS() =
    default;

MediaPerceptionAPIDelegateChromeOS::~MediaPerceptionAPIDelegateChromeOS() {}

void MediaPerceptionAPIDelegateChromeOS::LoadCrOSComponent(
    const extensions::api::media_perception_private::ComponentType& type,
    LoadCrOSComponentCallback load_callback) {
  g_browser_process->platform_part()->cros_component_manager()->Load(
      GetComponentNameForComponentType(type),
      component_updater::CrOSComponentManager::MountPolicy::kMount,
      component_updater::CrOSComponentManager::UpdatePolicy::kDontForce,
      base::BindOnce(OnLoadComponent, std::move(load_callback)));
}

void MediaPerceptionAPIDelegateChromeOS::
    BindDeviceFactoryProviderToVideoCaptureService(
        video_capture::mojom::DeviceFactoryProviderPtr* provider) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // In unit test environments, there may not be any connector.
  content::ServiceManagerConnection* connection =
      content::ServiceManagerConnection::GetForProcess();
  if (!connection)
    return;
  service_manager::Connector* connector = connection->GetConnector();
  if (!connector)
    return;
  connector->BindInterface(video_capture::mojom::kServiceName, provider);

  video_capture::mojom::AcceleratorFactoryPtr accelerator_factory;
  mojo::MakeStrongBinding(
      std::make_unique<
          content::DelegateToBrowserGpuServiceAcceleratorFactory>(),
      mojo::MakeRequest(&accelerator_factory));
  (*provider)->InjectGpuDependencies(std::move(accelerator_factory));
}

void MediaPerceptionAPIDelegateChromeOS::SetMediaPerceptionRequestHandler(
    MediaPerceptionRequestHandler handler) {
  handler_ = std::move(handler);
}

void MediaPerceptionAPIDelegateChromeOS::ForwardMediaPerceptionRequest(
    chromeos::media_perception::mojom::MediaPerceptionRequest request,
    content::RenderFrameHost* render_frame_host) {
  if (!handler_) {
    DLOG(ERROR) << "Got request but the handler is not set.";
    return;
  }
  handler_.Run(std::move(request));
}

}  // namespace extensions
