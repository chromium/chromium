// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/libvda_service_provider.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/ash/arc/video/gpu_arc_video_service_host.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

LibvdaServiceProvider::LibvdaServiceProvider() {}

LibvdaServiceProvider::~LibvdaServiceProvider() = default;

void LibvdaServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      libvda::kLibvdaServiceInterface, libvda::kProvideMojoConnectionMethod,
      base::BindRepeating(&LibvdaServiceProvider::ProvideMojoConnection,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&LibvdaServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
}

void LibvdaServiceProvider::OnExported(const std::string& interface_name,
                                       const std::string& method_name,
                                       bool success) {
  LOG_IF(ERROR, !success) << "Failed to export " << interface_name << "."
                          << method_name;
}

void LibvdaServiceProvider::ProvideMojoConnection(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  arc::GpuArcVideoServiceHost* gpu_arc_video_service_host =
      arc::GpuArcVideoServiceHost::Get();
  gpu_arc_video_service_host->OnBootstrapVideoAcceleratorFactory(base::BindOnce(
      &LibvdaServiceProvider::OnBootstrapVideoAcceleratorFactoryCallback,
      weak_ptr_factory_.GetWeakPtr(), method_call, std::move(response_sender)));
}

void LibvdaServiceProvider::OnBootstrapVideoAcceleratorFactoryCallback(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender,
    mojo::ScopedHandle handle,
    const std::string& pipe_name) {
  base::ScopedFD fd = mojo::UnwrapPlatformHandle(std::move(handle)).TakeFD();

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendFileDescriptor(fd.get());
  writer.AppendString(pipe_name);
  std::move(response_sender).Run(std::move(response));
}

}  // namespace ash
