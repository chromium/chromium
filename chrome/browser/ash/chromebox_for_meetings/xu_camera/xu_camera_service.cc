// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/xu_camera/xu_camera_service.h"

#include <base/notreached.h>
#include <errno.h>
#include <utility>

#include "chromeos/ash/components/dbus/chromebox_for_meetings/cfm_hotline_client.h"

namespace ash::cfm {

namespace {

XuCameraService* g_xu_camera_service = nullptr;

}  // namespace

XuCameraService::XuCameraService()
    : service_adaptor_(mojom::XuCamera::Name_, this) {
  CfmHotlineClient::Get()->AddObserver(this);
}

XuCameraService::~XuCameraService() {
  CfmHotlineClient::Get()->RemoveObserver(this);
}

void XuCameraService::Initialize() {
  CHECK(!g_xu_camera_service);
  g_xu_camera_service = new XuCameraService();
}

void XuCameraService::Shutdown() {
  CHECK(g_xu_camera_service);
  delete g_xu_camera_service;
  g_xu_camera_service = nullptr;
}

XuCameraService* XuCameraService::Get() {
  return g_xu_camera_service;
}

bool XuCameraService::IsInitialized() {
  return g_xu_camera_service;
}

bool XuCameraService::ServiceRequestReceived(
    const std::string& interface_name) {
  if (interface_name != mojom::XuCamera::Name_) {
    return false;
  }
  service_adaptor_.BindServiceAdaptor();
  return true;
}

void XuCameraService::OnBindService(
    mojo::ScopedMessagePipeHandle receiver_pipe) {
  receivers_.Add(
      this, mojo::PendingReceiver<mojom::XuCamera>(std::move(receiver_pipe)));
}

void XuCameraService::OnAdaptorDisconnect() {
  receivers_.Clear();
}

// mojom:XuCamera implementation boilerplate
void XuCameraService::GetUnitId(const mojom::WebcamIdPtr id,
                                const std::vector<uint8_t>& guid,
                                GetUnitIdCallback callback) {
  NOTIMPLEMENTED();
  guid_ = guid;
  std::move(callback).Run(ENOSYS, '0');
}

void XuCameraService::MapCtrl(const mojom::WebcamIdPtr id,
                              const mojom::ControlMappingPtr mapping_ctrl,
                              MapCtrlCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(ENOSYS);
}

void XuCameraService::GetCtrl(const mojom::WebcamIdPtr id,
                              const mojom::CtrlTypePtr ctrl,
                              const mojom::GetFn fn,
                              GetCtrlCallback callback) {
  NOTIMPLEMENTED();
  std::vector<uint8_t> vec;
  std::move(callback).Run(ENOSYS, vec);
}

void XuCameraService::SetCtrl(const mojom::WebcamIdPtr id,
                              const mojom::CtrlTypePtr ctrl,
                              const std::vector<uint8_t>& data,
                              SetCtrlCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(ENOSYS);
}

}  // namespace ash::cfm
