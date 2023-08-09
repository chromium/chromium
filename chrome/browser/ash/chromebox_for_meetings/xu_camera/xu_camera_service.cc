// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/xu_camera/xu_camera_service.h"

#include <base/notreached.h>
#include <base/posix/eintr_wrapper.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/usb/video.h>
#include <linux/uvcvideo.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <cstdint>
#include <utility>

#include "chromeos/ash/components/dbus/chromebox_for_meetings/cfm_hotline_client.h"
#include "xu_camera_service.h"

namespace ash::cfm {

namespace {

class RealDelegate : public XuCameraService::Delegate {
 public:
  RealDelegate() = default;
  RealDelegate(const RealDelegate&) = delete;
  RealDelegate& operator=(const RealDelegate&) = delete;

  int Ioctl(int fd, int request, uvc_xu_control_query* query) override {
    return HANDLE_EINTR(ioctl(fd, request, query));
  }

  int OpenFile(std::string path) override {
    int fd = open(path.c_str(), O_RDWR | O_NONBLOCK, 0);
    if (fd == -1) {
      LOG(ERROR) << "Failed to open device. Open file failed. " << path;
    }
    VLOG(4) << "File path: " << path;
    return fd;
  }

  void CloseFile(int file_descriptor) override {
    if (file_descriptor >= 0) {
      VLOG(4) << "Close file path: " << file_descriptor;
      close(file_descriptor);
    }
  }
};

XuCameraService* g_xu_camera_service = nullptr;

}  // namespace

XuCameraService::XuCameraService(Delegate* delegate)
    : delegate_(delegate), service_adaptor_(mojom::XuCamera::Name_, this) {
  CfmHotlineClient::Get()->AddObserver(this);
}

XuCameraService::~XuCameraService() {
  CfmHotlineClient::Get()->RemoveObserver(this);
}

void XuCameraService::Initialize() {
  CHECK(!g_xu_camera_service);
  g_xu_camera_service = new XuCameraService(new RealDelegate());
}

void XuCameraService::InitializeForTesting(Delegate* delegate) {
  CHECK(!g_xu_camera_service);
  g_xu_camera_service = new XuCameraService(delegate);
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

uint8_t XuCameraService::GetRequest(const mojom::GetFn& fn) {
  switch (fn) {
    case mojom::GetFn::kCur:
      return UVC_GET_CUR;
    case mojom::GetFn::kMin:
      return UVC_GET_MIN;
    case mojom::GetFn::kMax:
      return UVC_GET_MAX;
    case mojom::GetFn::kDef:
      return UVC_GET_DEF;
    case mojom::GetFn::kRes:
      return UVC_GET_RES;
    case mojom::GetFn::kLen:
      return UVC_GET_LEN;
    case mojom::GetFn::kInfo:
      return UVC_GET_INFO;
    default:
      LOG(ERROR) << __func__ << ": Invalid GetFn. ";
      return EINVAL;
  }
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

void XuCameraService::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
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
  uint8_t error_code = ENOSYS;
  std::vector<uint8_t> data;
  mojom::ControlQueryPtr query;
  std::string dev_path = id->is_device_id() ? GetDevicePath(id->get_device_id())
                                            : id->get_dev_path();

  int file_descriptor = delegate_->OpenFile(dev_path);
  if (file_descriptor < 0) {
    LOG(ERROR) << __func__ << ": File is invalid";
    std::move(callback).Run(ENOENT, data);
    return;
  }

  // GetCtrl depending on whether id provided is WebRTC or filepath
  switch (ctrl->which()) {
    case mojom::CtrlType::Tag::kQueryCtrl:
      error_code =
          CtrlThroughQuery(file_descriptor, std::move(ctrl->get_query_ctrl()),
                           data, GetRequest(fn));
      break;
    case mojom::CtrlType::Tag::kMappingCtrl:
      NOTIMPLEMENTED();
      error_code = ENOSYS;
      break;
    default:
      LOG(ERROR) << __func__ << ": Invalid CtrlType::Tag";
      error_code = EINVAL;
  }
  delegate_->CloseFile(file_descriptor);
  std::move(callback).Run(error_code, data);
}

void XuCameraService::SetCtrl(const mojom::WebcamIdPtr id,
                              const mojom::CtrlTypePtr ctrl,
                              const std::vector<uint8_t>& data,
                              SetCtrlCallback callback) {
  uint8_t error_code = ENOSYS;
  mojom::ControlQueryPtr query;
  std::string dev_path = id->is_device_id() ? GetDevicePath(id->get_device_id())
                                            : id->get_dev_path();
  int file_descriptor = delegate_->OpenFile(dev_path);
  if (file_descriptor < 0) {
    LOG(ERROR) << __func__ << ": File is invalid";
    std::move(callback).Run(ENOENT);
    return;
  }

  std::vector<uint8_t> data_(data);
  // SetCtrl depending on whether id provided is WebRTC or filepath
  switch (ctrl->which()) {
    case mojom::CtrlType::Tag::kQueryCtrl:
      error_code =
          CtrlThroughQuery(file_descriptor, std::move(ctrl->get_query_ctrl()),
                           data_, UVC_SET_CUR);
      break;
    case mojom::CtrlType::Tag::kMappingCtrl:
      NOTIMPLEMENTED();
      error_code = ENOSYS;
      break;
    default:
      LOG(ERROR) << __func__ << ": Invalid CtrlType::Tag";
      error_code = EINVAL;
  }

  delegate_->CloseFile(file_descriptor);
  std::move(callback).Run(error_code);
}

uint8_t XuCameraService::QueryXuControl(int file_descriptor,
                                        uint8_t unit_id,
                                        uint8_t selector,
                                        uint8_t* data,
                                        uint8_t query_request,
                                        uint16_t size) {
  struct uvc_xu_control_query control_query;
  control_query.unit = unit_id;
  control_query.selector = selector;
  control_query.query = query_request;
  control_query.size = size;
  control_query.data = data;
  int error =
      delegate_->Ioctl(file_descriptor, UVCIOC_CTRL_QUERY, &control_query);

  if (error < 0) {
    LOG(ERROR) << "ioctl call failed. error: " << error;
    return errno;
  }
  return error;
}

std::string XuCameraService::GetDevicePath(const std::string& device_id) {
  // Not implemented
  NOTIMPLEMENTED();
  return "";
}

uint8_t XuCameraService::CtrlThroughQuery(int file_descriptor,
                                          const mojom::ControlQueryPtr& query,
                                          std::vector<uint8_t>& data,
                                          unsigned int request) {
  if (UVC_SET_CUR == request) {
    uint8_t error_code =
        QueryXuControl(file_descriptor, query->unit_id, query->selector,
                       data.data(), request, data.size());
    return error_code;
  }

  data.clear();
  data.resize(2);
  uint8_t error_code =
      QueryXuControl(file_descriptor, query->unit_id, query->selector,
                     data.data(), UVC_GET_LEN, sizeof(uint16_t));

  if (error_code != 0 || UVC_GET_LEN == request) {
    return error_code;
  }

  // Use the queried data as data length for GetCtrl.
  // UVC_GET_LEN return values is always returned as little-endian 16-bit
  // integer by the device.
  uint16_t data_len = le16toh(data[0] | (data[1] << 8));
  data.clear();
  data.resize(data_len);

  error_code = QueryXuControl(file_descriptor, query->unit_id, query->selector,
                              data.data(), request, data_len);

  return error_code;
}
}  // namespace ash::cfm
