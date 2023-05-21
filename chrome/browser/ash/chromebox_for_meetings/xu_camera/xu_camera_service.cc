// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/xu_camera/xu_camera_service.h"

#include <base/notreached.h>
#include <base/posix/eintr_wrapper.h>
#include <errno.h>
#include <fcntl.h>
#include <libudev.h>
#include <linux/usb/video.h>
#include <linux/uvcvideo.h>
#include <linux/videodev2.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <cstdint>
#include <utility>

#include "chromeos/ash/components/dbus/chromebox_for_meetings/cfm_hotline_client.h"
#include "content/public/browser/device_service.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"

namespace ash::cfm {

namespace {
static constexpr int kGuidSize = 16;
static constexpr int kSubtypeOffset = 2;
static constexpr int kVideoClass = 14;
static constexpr int kVideoSubclass = 1;
static constexpr int kXUSubtype = 6;

typedef struct {
  uint8_t klength;
  uint8_t kType;
  uint8_t kSubtype;
  uint8_t kUnitId;
  uint8_t kGuid[16];
} kXuInterface;

class RealDelegate : public XuCameraService::Delegate {
 public:
  RealDelegate() = default;
  RealDelegate(const RealDelegate&) = delete;
  RealDelegate& operator=(const RealDelegate&) = delete;

  int Ioctl(const base::ScopedFD& fd,
            unsigned int request,
            void* query) override {
    VLOG(4) << __func__ << " with request: " << request;
    if (!fd.is_valid()) {
      LOG(ERROR) << "File Descriptor No longer valid";
      return EBADF;
    }
    return HANDLE_EINTR(ioctl(fd.get(), request, query));
  }

  bool OpenFile(base::ScopedFD& fd, const std::string& path) override {
    fd.reset(open(path.c_str(), O_RDWR | O_NONBLOCK, 0));
    VLOG(4) << __func__ << "File path: " << path;
    LOG_IF(ERROR, !fd.is_valid())
        << "Failed to open device. Open file failed. " << path;
    return fd.is_valid();
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

void XuCameraService::GetUnitId(const mojom::WebcamIdPtr id,
                                const std::vector<uint8_t>& guid,
                                GetUnitIdCallback callback) {
  // TODO(b/260593636): Leverage WebRTC and GetDevicePath() once implemented
  auto unitId = guid_unitid_map_.find(guid);
  if (unitId != guid_unitid_map_.end()) {
    VLOG(4) << __func__
            << ": UnitId found: " << static_cast<char>(unitId->second);
    std::move(callback).Run(0, unitId->second);
    return;
  }

  content::GetDeviceService().BindUsbDeviceManager(
      usb_manager_.BindNewPipeAndPassReceiver());
  device::mojom::UsbEnumerationOptionsPtr options =
      device::mojom::UsbEnumerationOptions::New();
  usb_manager_->GetDevices(
      std::move(options),
      base::BindOnce(&XuCameraService::OnGetDevices, weak_factory_.GetWeakPtr(),
                     guid, std::move(callback)));
}

void XuCameraService::OnGetDevices(
    const std::vector<uint8_t>& guid,
    GetUnitIdCallback callback,
    std::vector<device::mojom::UsbDeviceInfoPtr> devices) {
  for (const auto& device_info : devices) {
    for (const auto& config : device_info->configurations) {
      for (const auto& interface : config->interfaces) {
        for (const auto& alternate : interface->alternates) {
          if (alternate->class_code == kVideoClass &&
              alternate->subclass_code == kVideoSubclass) {
            /* extra_data.data() is additional raw data in byte form
             * consisting of an array of interfaces. */
            uint8_t* data_ptr = alternate->extra_data.data();
            int end = alternate->extra_data.size();
            int cur = 0;
            kXuInterface curXuInterface;
            while ((cur + kSubtypeOffset) < end) {
              if (static_cast<int>(data_ptr[cur + kSubtypeOffset]) ==
                      kXUSubtype &&
                  (cur + (int)sizeof(curXuInterface)) < end) {
                std::memcpy(&curXuInterface, &data_ptr[cur],
                            sizeof(curXuInterface));
                guid_unitid_map_.insert({ProcessGuid(curXuInterface.kGuid),
                                         curXuInterface.kUnitId});
              }
              cur += static_cast<int>(data_ptr[cur]);
            }
          }
        }
      }
    }
  }

  auto unitId = guid_unitid_map_.find(guid);
  if (unitId != guid_unitid_map_.end()) {
    VLOG(4) << __func__
            << ": UnitId found: " << static_cast<char>(unitId->second);
    std::move(callback).Run(0, unitId->second);
    return;
  }

  VLOG(4) << __func__ << ": UnitId not found";
  std::move(callback).Run(ENOSYS, '0');
}

std::vector<uint8_t> XuCameraService::ProcessGuid(
    uint8_t unprocessed_guid[kGuidSize]) {
  std::vector<uint8_t> guid(kGuidSize);
  /* GUID consist of 5 combined values
   * [0-3], [4-5], [6-7]. [8-9]. [10-15]
   * The first 3 values are stored in little Endian and need to be
   * converted to big endian form the correct GUID
   */
  guid[0] = unprocessed_guid[3];
  guid[1] = unprocessed_guid[2];
  guid[2] = unprocessed_guid[1];
  guid[3] = unprocessed_guid[0];
  guid[4] = unprocessed_guid[5];
  guid[5] = unprocessed_guid[4];
  guid[6] = unprocessed_guid[7];
  guid[7] = unprocessed_guid[6];
  guid[8] = unprocessed_guid[8];
  guid[9] = unprocessed_guid[9];
  guid[10] = unprocessed_guid[10];
  guid[11] = unprocessed_guid[11];
  guid[12] = unprocessed_guid[12];
  guid[13] = unprocessed_guid[13];
  guid[14] = unprocessed_guid[14];
  guid[15] = unprocessed_guid[15];
  return guid;
}

void XuCameraService::MapCtrl(const mojom::WebcamIdPtr id,
                              const mojom::ControlMappingPtr mapping_ctrl,
                              MapCtrlCallback callback) {
  uint8_t error_code = 0;
  std::string dev_path = id->is_device_id() ? GetDevicePath(id->get_device_id())
                                            : id->get_dev_path();
  VLOG(4) << __func__ << ": dev_path - " << dev_path;
  base::ScopedFD file_descriptor;
  if (!delegate_->OpenFile(file_descriptor, dev_path)) {
    LOG(ERROR) << __func__ << ": File is invalid";
    std::move(callback).Run(ENOENT);
    return;
  }

  struct uvc_menu_info uvc_menus[mapping_ctrl->menu_entries->menu_info.size()];

  int index = 0;
  for (auto menu_info = mapping_ctrl->menu_entries->menu_info.begin();
       menu_info < mapping_ctrl->menu_entries->menu_info.end(); menu_info++) {
    const struct uvc_menu_info info = {
        .value = (*menu_info)->value,
        .name = {*((*menu_info)->name.data())},
    };
    uvc_menus[index] = info;
    index++;
  }

  struct uvc_xu_control_mapping control_mapping = {
      .id = mapping_ctrl->id,
      .name = {*(mapping_ctrl->name.data())},
      .entity = {*(mapping_ctrl->guid.data())},
      .selector = mapping_ctrl->selector,
      .size = mapping_ctrl->size,
      .offset = mapping_ctrl->offset,
      .v4l2_type = mapping_ctrl->v4l2_type,
      .data_type = mapping_ctrl->data_type,
      .menu_info = uvc_menus,
      .menu_count = static_cast<uint32_t>(index),
  };

  // Map the controls to v4l2
  error_code =
      delegate_->Ioctl(file_descriptor, UVCIOC_CTRL_MAP, &control_mapping);

  std::move(callback).Run(error_code);
}

void XuCameraService::GetCtrl(const mojom::WebcamIdPtr id,
                              const mojom::CtrlTypePtr ctrl,
                              const mojom::GetFn fn,
                              GetCtrlCallback callback) {
  uint8_t error_code = 0;
  std::vector<uint8_t> data;
  std::string dev_path = id->is_device_id() ? GetDevicePath(id->get_device_id())
                                            : id->get_dev_path();

  VLOG(4) << __func__ << ": dev_path - " << dev_path;
  base::ScopedFD file_descriptor;
  if (!delegate_->OpenFile(file_descriptor, dev_path)) {
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
      error_code = CtrlThroughMapping(
          file_descriptor, std::move(ctrl->get_mapping_ctrl()), data, fn);
      break;
    default:
      LOG(ERROR) << __func__ << ": Invalid CtrlType::Tag";
      error_code = EINVAL;
  }
  std::move(callback).Run(error_code, data);
}

void XuCameraService::SetCtrl(const mojom::WebcamIdPtr id,
                              const mojom::CtrlTypePtr ctrl,
                              const std::vector<uint8_t>& data,
                              SetCtrlCallback callback) {
  uint8_t error_code = 0;
  std::string dev_path = id->is_device_id() ? GetDevicePath(id->get_device_id())
                                            : id->get_dev_path();
  VLOG(4) << __func__ << ": dev_path - " << dev_path;
  base::ScopedFD file_descriptor;
  if (!delegate_->OpenFile(file_descriptor, dev_path)) {
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
    case mojom::CtrlType::Tag::kMappingCtrl: {
      mojom::ControlMappingPtr mapping = std::move(ctrl->get_mapping_ctrl());
      int32_t newValue;
      CopyFromData(&newValue, data_);
      struct v4l2_control control = {.id = mapping->id, .value = newValue};
      error_code = delegate_->Ioctl(file_descriptor, VIDIOC_S_CTRL, &control);
      break;
    }
    default:
      LOG(ERROR) << __func__ << ": Invalid CtrlType::Tag";
      error_code = EINVAL;
  }

  std::move(callback).Run(error_code);
}

uint8_t XuCameraService::QueryXuControl(const base::ScopedFD& file_descriptor,
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

  VLOG(4) << __func__ << ": unit_id -" << static_cast<unsigned int>(unit_id)
          << " selector - " << static_cast<unsigned int>(selector);
  int error =
      delegate_->Ioctl(file_descriptor, UVCIOC_CTRL_QUERY, &control_query);

  if (error < 0) {
    LOG(ERROR) << "ioctl call failed. error: " << errno;
    return errno;
  }
  return error;
}

std::string XuCameraService::GetDevicePath(const std::string& device_id) {
  // Not implemented
  NOTIMPLEMENTED();
  return "";
}

uint8_t XuCameraService::CtrlThroughQuery(const base::ScopedFD& file_descriptor,
                                          const mojom::ControlQueryPtr& query,
                                          std::vector<uint8_t>& data,
                                          const uint8_t& request) {
  VLOG(4) << __func__ << " request - " << static_cast<unsigned int>(request);
  uint8_t data_len;
  uint8_t error_code = 0;
  if (UVC_SET_CUR == request) {
    error_code =
        QueryXuControl(file_descriptor, query->unit_id, query->selector,
                       data.data(), request, data.size());
    return error_code;
  } else if (UVC_GET_INFO == request) {
    data_len = 1;
  } else {
    data.clear();
    data.resize(2);
    error_code = GetLength(data.data(), file_descriptor, query->unit_id,
                           query->selector);
    if (error_code != 0 || UVC_GET_LEN == request) {
      return error_code;
    }
    // Use the queried data as data length for GetCtrl.
    // UVC_GET_LEN return values is always returned as little-endian 16-bit
    // integer by the device.
    data_len = le16toh(data[0] | (data[1] << 8));
  }

  data.clear();
  data.resize(data_len);

  error_code = QueryXuControl(file_descriptor, query->unit_id, query->selector,
                              data.data(), request, data_len);
  VLOG(4) << __func__
          << "query data error_code: " << static_cast<unsigned int>(error_code);

  return error_code;
}

uint8_t XuCameraService::CtrlThroughMapping(
    const base::ScopedFD& file_descriptor,
    const mojom::ControlMappingPtr& mapping,
    std::vector<uint8_t>& data,
    const mojom::GetFn& fn) {
  uint8_t error_code = 0;

  VLOG(4) << __func__ << " GetFn - " << fn;
  // Early return for kCur/kLen  vs other info that requires VIDIOC_QUERYCTRL
  if (mojom::GetFn::kLen == fn) {
    // User set up the map so they should know that the size returned will be
    // in bits.
    data.push_back(mapping->size);
    return error_code;
  } else if (mojom::GetFn::kCur == fn) {
    struct v4l2_control control = {.id = mapping->id};
    error_code = delegate_->Ioctl(file_descriptor, VIDIOC_G_CTRL, &control);
    CopyToData<int32_t>(&control.value, data, sizeof(control.value));
    return error_code;
  }

  struct v4l2_queryctrl query = {
      .id = mapping->id,
  };

  error_code = delegate_->Ioctl(file_descriptor, VIDIOC_QUERYCTRL, &query);

  if (error_code != 0) {
    LOG(ERROR) << __func__ << " VIDIOC_QUERYCTRL error_code - " << error_code;
    return error_code;
  }

  switch (fn) {
    case mojom::GetFn::kMin: {
      CopyToData<int32_t>(&query.minimum, data, sizeof(query.minimum));
      break;
    }
    case mojom::GetFn::kMax: {
      CopyToData<int32_t>(&query.maximum, data, sizeof(query.maximum));
      break;
    }
    case mojom::GetFn::kDef: {
      CopyToData<int32_t>(&query.default_value, data,
                          sizeof(query.default_value));
      break;
    }
    case mojom::GetFn::kRes: {
      CopyToData<int32_t>(&query.step, data, sizeof(query.step));
      break;
    }
    case mojom::GetFn::kInfo: {
      // Get control info bitmap for get/set
      CopyToData<uint32_t>(&query.flags, data, sizeof(query.flags));
      break;
    }
    default:
      LOG(ERROR) << __func__ << ": Invalid GetFn. ";
      return EINVAL;
  }
  VLOG(4) << __func__ << ": Success query";
  return error_code;
}

template <typename T>
void XuCameraService::CopyToData(T* value,
                                 std::vector<uint8_t>& data,
                                 size_t size) {
  VLOG(4) << __func__ << " of size " << size;
  data.reserve(size);
  uint8_t* valueAsUint8 = reinterpret_cast<uint8_t*>(value);
  for (size_t i = 0; i < size; ++i) {
    data.push_back(*valueAsUint8);
    valueAsUint8++;
  }
}

template <typename T>
void XuCameraService::CopyFromData(T* value, std::vector<uint8_t>& data) {
  int shiftBit = 0;
  for (size_t i = 0; i < data.size(); ++i) {
    *value += data[i] << shiftBit;
    shiftBit += 8;
  }
}

uint8_t XuCameraService::GetLength(uint8_t* data,
                                   const base::ScopedFD& file_descriptor,
                                   const uint8_t& unit_id,
                                   const uint8_t& selector) {
  // UVC_GET_LEN is always size of 2
  uint8_t error_code =
      QueryXuControl(file_descriptor, unit_id, selector, data, UVC_GET_LEN, 2);
  VLOG_IF(4, (error_code != 0))
      << __func__
      << "query length error_code: " << static_cast<unsigned int>(error_code);
  return error_code;
}

}  // namespace ash::cfm
