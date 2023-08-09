// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_XU_CAMERA_XU_CAMERA_SERVICE_H_
#define CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_XU_CAMERA_XU_CAMERA_SERVICE_H_

#include <string>

#include "chrome/browser/ash/chromebox_for_meetings/service_adaptor.h"
#include "chromeos/ash/components/dbus/chromebox_for_meetings/cfm_observer.h"
#include "chromeos/ash/services/chromebox_for_meetings/public/mojom/xu_camera.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash::cfm {

// Implementation of the XuCamera Service
// Allows CfM to control non-standard camera functionality.
class XuCameraService : public CfmObserver,
                        public ServiceAdaptor::Delegate,
                        public mojom::XuCamera {
 public:
  ~XuCameraService() override;

  XuCameraService(const XuCameraService&) = delete;
  XuCameraService& operator=(const XuCameraService&) = delete;

  // Manage singleton instance.
  static void Initialize();
  static void Shutdown();
  static XuCameraService* Get();
  static bool IsInitialized();

  // CfmObserver implementation
  bool ServiceRequestReceived(const std::string& interface_name) override;

  // ServiceAdaptorDelegate implementation
  void OnBindService(mojo::ScopedMessagePipeHandle receiver_pipe) override;
  void OnAdaptorDisconnect() override;

  // mojom:XuCamera implementation
  void GetUnitId(const mojom::WebcamIdPtr id,
                 const std::vector<uint8_t>& guid,
                 GetUnitIdCallback callback) override;
  void MapCtrl(const mojom::WebcamIdPtr id,
               const mojom::ControlMappingPtr mapping_ctrl,
               MapCtrlCallback callback) override;
  void GetCtrl(const mojom::WebcamIdPtr id,
               const mojom::CtrlTypePtr ctrl,
               const mojom::GetFn fn,
               GetCtrlCallback callback) override;
  void SetCtrl(const mojom::WebcamIdPtr id,
               const mojom::CtrlTypePtr ctrl,
               const std::vector<uint8_t>& data,
               SetCtrlCallback callback) override;

 private:
  XuCameraService();

  ServiceAdaptor service_adaptor_;
  mojo::ReceiverSet<XuCamera> receivers_;
  std::vector<uint8_t> guid_;
};

}  // namespace ash::cfm

#endif  // CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_XU_CAMERA_XU_CAMERA_SERVICE_H_
