// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_XU_CAMERA_XU_CAMERA_SERVICE_H_
#define CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_XU_CAMERA_XU_CAMERA_SERVICE_H_

#include <linux/usb/video.h>
#include <linux/uvcvideo.h>
#include <linux/videodev2.h>

#include <cstdint>
#include <string>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/chromebox_for_meetings/service_adaptor.h"
#include "chromeos/ash/components/dbus/chromebox_for_meetings/cfm_observer.h"
#include "chromeos/ash/services/chromebox_for_meetings/public/mojom/xu_camera.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/usb_manager.mojom.h"
#include "services/device/public/mojom/usb_manager_client.mojom.h"

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
    virtual int Ioctl(const base::ScopedFD& fd,
                      unsigned int request,
                      void* query) = 0;

    // Open file given the file path and return the file descriptor.
    virtual bool OpenFile(base::ScopedFD& fd, const std::string& path) = 0;
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
  uint8_t QueryXuControl(const base::ScopedFD& file_descriptor,
                         uint8_t unit_id,
                         uint8_t selector,
                         uint8_t* data,
                         uint8_t query_request,
                         uint16_t size);
  std::string GetDevicePath(const std::string& device_id);
  uint8_t CtrlThroughQuery(const base::ScopedFD& file_descriptor,
                           const mojom::ControlQueryPtr& query,
                           std::vector<uint8_t>& data,
                           const uint8_t& query_request);
  uint8_t CtrlThroughMapping(const base::ScopedFD& file_descriptor,
                             const mojom::ControlMappingPtr& mapping,
                             std::vector<uint8_t>& data,
                             const mojom::GetFn& fn);
  void ConvertLength(std::vector<uint8_t>& data, uint32_t type);
  template <typename T>
  void CopyToData(T* value, std::vector<uint8_t>& data, size_t size);
  template <typename T>
  void CopyFromData(T* value, std::vector<uint8_t>& data);
  uint8_t GetLength(uint8_t* data,
                    const base::ScopedFD& file_descriptor,
                    const uint8_t& unit_id,
                    const uint8_t& selector);
  void OnGetDevices(const std::vector<uint8_t>& guid,
                    GetUnitIdCallback callback,
                    std::vector<device::mojom::UsbDeviceInfoPtr> devices);
  std::vector<uint8_t> ProcessGuid(uint8_t unprocessed_guid[16]);
  Delegate* delegate_;
  ServiceAdaptor service_adaptor_;
  mojo::ReceiverSet<XuCamera> receivers_;
  mojo::Remote<device::mojom::UsbDeviceManager> usb_manager_;
  std::map<std::vector<uint8_t>, uint8_t> guid_unitid_map_ = {};
  base::WeakPtrFactory<XuCameraService> weak_factory_{this};
};

}  // namespace ash::cfm

#endif  // CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_XU_CAMERA_XU_CAMERA_SERVICE_H_
