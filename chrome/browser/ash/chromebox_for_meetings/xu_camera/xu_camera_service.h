// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_XU_CAMERA_XU_CAMERA_SERVICE_H_
#define CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_XU_CAMERA_XU_CAMERA_SERVICE_H_

#include <linux/usb/video.h>
#include <linux/uvcvideo.h>

#include <cstdint>
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
  // Delegate interface to handle file-related operations.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // System call for device input/output operations.
    virtual int Ioctl(int fd, int request, uvc_xu_control_query* query) = 0;

    // Open file given the file path and return the file descriptor.
    virtual int OpenFile(std::string path) = 0;

    // Close file given the file descriptor.
    virtual void CloseFile(int file_descriptor) = 0;
  };

  ~XuCameraService() override;

  XuCameraService(const XuCameraService&) = delete;
  XuCameraService& operator=(const XuCameraService&) = delete;

  // Manage singleton instance.
  static void Initialize();
  static void InitializeForTesting(Delegate* delegate);
  static void Shutdown();
  static XuCameraService* Get();
  static bool IsInitialized();
  static uint8_t GetRequest(const mojom::GetFn& fn);

 protected:
  // If nullptr is passed the default Delegate will be used
  explicit XuCameraService(Delegate* delegate_);

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

  // Set the XuCameraService::Delegate
  void SetDelegate(Delegate* delegate);

 private:
  uint8_t QueryXuControl(int file_descriptor,
                         uint8_t unit_id,
                         uint8_t selector,
                         uint8_t* data,
                         uint8_t query_request,
                         uint16_t size);
  std::string GetDevicePath(const std::string& device_id);
  uint8_t CtrlThroughQuery(int file_descriptor,
                           const mojom::ControlQueryPtr& query,
                           std::vector<uint8_t>& data,
                           unsigned int request);

  Delegate* delegate_;
  ServiceAdaptor service_adaptor_;
  mojo::ReceiverSet<XuCamera> receivers_;
  std::vector<uint8_t> guid_;
  std::map<std::vector<uint8_t>, uint8_t> guid_unitid_map_ = {};
};

}  // namespace ash::cfm

#endif  // CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_XU_CAMERA_XU_CAMERA_SERVICE_H_
