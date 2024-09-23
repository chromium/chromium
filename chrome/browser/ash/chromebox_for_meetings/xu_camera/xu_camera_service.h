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
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/chromebox_for_meetings/cfm_observer.h"
#include "chromeos/dbus/ip_peripheral/ip_peripheral_service_client.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/service_adaptor.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/xu_camera.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/usb_manager.mojom.h"
#include "services/device/public/mojom/usb_manager_client.mojom.h"

namespace content {
struct GlobalRenderFrameHostId;
}  // namespace content

namespace ash::cfm {

// Implementation of the XuCamera Service
// Allows CfM to control non-standard camera functionality.
class XuCameraService : public CfmObserver,
                        public chromeos::cfm::ServiceAdaptor::Delegate,
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

  // Called when attempting to Bind a mojom using using a message pipe of the
  // given types PendingReceiver |receiver_pipe|.
  // |content::GlobalRenderFrameHostId| used to dereference hashed device id.
  // Note: Only the user facing usecases requires this value; other uses
  // of this service will not require a valid |content::GlobalRenderFrameHostId|
  void BindServiceContext(mojo::PendingReceiver<mojom::XuCamera> receiver,
                          const content::GlobalRenderFrameHostId& id);

 protected:
  // If nullptr is passed the default Delegate will be used
  explicit XuCameraService(Delegate* delegate);

  // CfmObserver implementation
  bool ServiceRequestReceived(const std::string& interface_name) override;

  // chromeos::cfm::ServiceAdaptor::Delegate implementation
  void OnBindService(mojo::ScopedMessagePipeHandle receiver_pipe) override;
  void OnAdaptorDisconnect() override;

  // mojom:XuCamera implementation
  void GetUnitId(mojom::WebcamIdPtr id,
                 const std::vector<uint8_t>& guid_le,
                 GetUnitIdCallback callback) override;
  void MapCtrl(mojom::WebcamIdPtr id,
               mojom::ControlMappingPtr mapping_ctrl,
               MapCtrlCallback callback) override;
  void GetCtrl(mojom::WebcamIdPtr id,
               mojom::CtrlTypePtr ctrl,
               mojom::GetFn fn,
               GetCtrlCallback callback) override;
  void SetCtrl(mojom::WebcamIdPtr id,
               mojom::CtrlTypePtr ctrl,
               const std::vector<uint8_t>& data,
               SetCtrlCallback callback) override;

  // Set the XuCameraService::Delegate
  void SetDelegate(Delegate* delegate);

 private:
  void GetUnitIdWithDevicePath(const std::vector<uint8_t>& guid_le,
                               GetUnitIdCallback callback,
                               const std::optional<std::string>& dev_path);
  void MapCtrlWithDevicePath(mojom::ControlMappingPtr mapping_ctrl,
                             MapCtrlCallback callback,
                             const std::optional<std::string>& dev_path) const;
  void GetCtrlWithDevicePath(mojom::CtrlTypePtr ctrl,
                             mojom::GetFn fn,
                             GetCtrlCallback callback,
                             const std::optional<std::string>& dev_path) const;
  void SetCtrlWithDevicePath(mojom::CtrlTypePtr ctrl,
                             const std::vector<uint8_t>& data,
                             SetCtrlCallback callback,
                             const std::optional<std::string>& dev_path) const;
  uint8_t QueryXuControl(const base::ScopedFD& file_descriptor,
                         uint8_t unit_id,
                         uint8_t selector,
                         uint8_t* data,
                         uint8_t query_request,
                         uint16_t size) const;
  void GetDevicePath(mojom::WebcamIdPtr id,
                     const content::GlobalRenderFrameHostId& host_id,
                     base::OnceCallback<void(const std::optional<std::string>&)>
                         callback) const;
  void GetCtrlDbus(const std::string& dev_path_or_ip_addr,
                   const mojom::ControlQueryPtr& query,
                   const uint8_t& query_request,
                   GetCtrlCallback callback) const;
  void SetCtrlDbus(const std::string& dev_path_or_ip_addr,
                   const mojom::ControlQueryPtr& query,
                   const std::vector<uint8_t>& data,
                   SetCtrlCallback callback) const;
  uint8_t CtrlThroughQuery(const base::ScopedFD& file_descriptor,
                           const mojom::ControlQueryPtr& query,
                           std::vector<uint8_t>& data,
                           const uint8_t& query_request) const;
  uint8_t CtrlThroughMapping(const base::ScopedFD& file_descriptor,
                             const mojom::ControlMappingPtr& mapping,
                             std::vector<uint8_t>& data,
                             const mojom::GetFn& fn) const;
  template <typename T>
  void CopyToData(T* value, std::vector<uint8_t>& data, size_t size) const;
  template <typename T>
  void CopyFromData(T* value, const std::vector<uint8_t>& data) const;
  uint8_t GetLength(uint8_t* data,
                    const base::ScopedFD& file_descriptor,
                    const uint8_t& unit_id,
                    const uint8_t& selector) const;
  void OnGetDevices(const std::vector<uint8_t>& guid_le,
                    GetUnitIdCallback callback,
                    std::vector<device::mojom::UsbDeviceInfoPtr> devices);
  raw_ptr<Delegate> delegate_;
  chromeos::cfm::ServiceAdaptor service_adaptor_;
  mojo::ReceiverSet<XuCamera, content::GlobalRenderFrameHostId> receivers_;
  mojo::Remote<device::mojom::UsbDeviceManager> usb_manager_;
  std::map<std::vector<uint8_t>, uint8_t> guid_unitid_map_ = {};
  const std::vector<uint8_t> meet_xu_guid_le_;
  base::WeakPtrFactory<XuCameraService> weak_factory_{this};
};

}  // namespace ash::cfm

#endif  // CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_XU_CAMERA_XU_CAMERA_SERVICE_H_
