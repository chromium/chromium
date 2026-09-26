// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/xu_camera/xu_camera_service.h"

#include <fcntl.h>
#include <libudev.h>
#include <linux/usb/video.h>
#include <linux/uvcvideo.h>
#include <linux/videodev2.h>
#include <stdlib.h>
#include <sys/ioctl.h>

#include <algorithm>
#include <cstdint>
#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/types/fixed_array.h"
#include "chrome/browser/media/webrtc/media_device_salt_service_factory.h"
#include "chromeos/ash/components/dbus/chromebox_for_meetings/cfm_hotline_client.h"
#include "components/media_device_salt/media_device_salt_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/browser/render_frame_host.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"

using chromeos::IpPeripheralServiceClient;

namespace ash::cfm {

namespace {
static constexpr int kGuidSize = 16;
static constexpr size_t kSubtypeOffset = 2;
static constexpr int kVideoClass = 14;
static constexpr int kVideoSubclass = 1;
static constexpr int kXUSubtype = 6;

typedef struct {
  uint8_t kLength;
  uint8_t kType;
  uint8_t kSubtype;
  uint8_t kUnitId;
  uint8_t kGuidLe[kGuidSize];  // little-endian from camera
} kXuInterface;

static constexpr char kAllowedDevPathPrefix[] = "/dev/video";
static constexpr size_t kMaxDeviceIndexDigits = 5;
static constexpr char kLocalIpAddress[] = "192.168.";  // Series One peripherals
static const std::initializer_list<uint8_t> kMeetXuGuidLe = {
    0x24, 0xE9, 0xD7, 0x74,  // bytes 0-3 (little-endian)
    0xC9, 0x49,              // bytes 4-5 (little-endian)
    0x45, 0x4A,              // bytes 6-7 (little-endian)
    0x98, 0xA3, 0x8A, 0x9F, 0x60, 0x06, 0x1E,
    0x83,  // bytes 8-15 (byte array)
};

// Copies as many leading bytes of `src` as fit into `dst`. Bytes of `dst`
// beyond `src.size()` are left untouched.
void CopyTruncated(base::span<uint8_t> dst, base::span<const uint8_t> src) {
  dst.copy_prefix_from(src.first(std::min(dst.size(), src.size())));
}

}  // namespace

bool IsValidV4L2DevicePath(const std::string& dev_path) {
  if (!base::StartsWith(dev_path, kAllowedDevPathPrefix)) {
    return false;
  }
  base::FilePath file_path(dev_path);
  if (file_path.ReferencesParent() || !file_path.IsAbsolute()) {
    return false;
  }
  constexpr size_t kPrefixLen = sizeof(kAllowedDevPathPrefix) - 1;
  const size_t suffix_len = dev_path.size() - kPrefixLen;
  if (suffix_len == 0 || suffix_len > kMaxDeviceIndexDigits) {
    return false;
  }
  if (suffix_len > 1 && dev_path[kPrefixLen] == '0') {
    return false;
  }
  for (size_t i = kPrefixLen; i < dev_path.size(); ++i) {
    if (!base::IsAsciiDigit(dev_path[i])) {
      return false;
    }
  }
  unsigned int device_index = 0;
  if (!base::StringToUint(dev_path.substr(kPrefixLen), &device_index)) {
    return false;
  }
  return true;
}

bool IsIpCamera(const std::string& dev_path) {
  if (!base::StartsWith(dev_path, kLocalIpAddress)) {
    return false;
  }
  const std::vector<std::string_view> parts = base::SplitStringPiece(
      dev_path, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parts.size() != 4) {
    return false;
  }
  if (parts[0] != "192" || parts[1] != "168") {
    return false;
  }
  for (size_t i = 2; i < 4; ++i) {
    if (parts[i].empty() || parts[i].size() > 3) {
      return false;
    }
    if (parts[i].size() > 1 && parts[i][0] == '0') {
      return false;
    }
    for (char c : parts[i]) {
      if (!base::IsAsciiDigit(c)) {
        return false;
      }
    }
    unsigned int val = 0;
    if (!base::StringToUint(parts[i], &val) || val > 255) {
      return false;
    }
  }
  return true;
}

bool IsValidDevPath(const std::string& dev_path) {
  return IsValidV4L2DevicePath(dev_path) || IsIpCamera(dev_path);
}

namespace {

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
    if (!IsValidV4L2DevicePath(path)) {
      LOG(ERROR) << "Failed to open device. Invalid path: " << path;
      return false;
    }
    fd.reset(open(path.c_str(), O_RDWR | O_NONBLOCK, 0));
    VLOG(4) << __func__ << "File path: " << path;
    LOG_IF(ERROR, !fd.is_valid())
        << "Failed to open device. Open file failed. " << path;
    return fd.is_valid();
  }
};

IpPeripheralServiceClient::GetControlCallback ConvertGetCtrlCallbackForDbus(
    XuCameraService::GetCtrlCallback callback) {
  // Adapts a XuCameraService::GetCtrlCallback OnceCallback<void (uint8_t,
  // std::vector<unsigned char>)> to a
  // IpPeripheralServiceClient::GetControlCallback OnceCallback<void (bool,
  // std::vector<uint8_t>)> (Note difference in first argument.)
  return base::BindOnce(
      [](XuCameraService::GetCtrlCallback cb, bool success,
         std::vector<uint8_t> result) -> void {
        std::move(cb).Run(success ? 0 : EINVAL, std::move(result));
      },
      std::move(callback));
}

IpPeripheralServiceClient::SetControlCallback ConvertSetCtrlCallbackForDbus(
    XuCameraService::SetCtrlCallback callback) {
  // Adapts a XuCameraService::SetCtrlCallback OnceCallback<void (uint8_t)>
  // to a IpPeripheralServiceClient::SetControlCallback OnceCallback<void
  // (bool)> (Note difference in first argument.)
  return base::BindOnce(
      [](XuCameraService::SetCtrlCallback cb, bool success) -> void {
        std::move(cb).Run(success ? 0 : EINVAL);
      },
      std::move(callback));
}

void TranslateDeviceId(
    const std::string& hashed_device_id,
    base::OnceCallback<void(const std::optional<std::string>&)> callback,
    const url::Origin& security_origin,
    const std::string& salt) {
  auto validate_and_run_callback = base::BindOnce(
      [](base::OnceCallback<void(const std::optional<std::string>&)> cb,
         const std::optional<std::string>& dev_path) {
        if (dev_path && !IsValidDevPath(*dev_path)) {
          LOG(ERROR) << "TranslateDeviceId resolved invalid device path.";
          std::move(cb).Run(std::nullopt);
          return;
        }
        std::move(cb).Run(dev_path);
      },
      std::move(callback));

  auto translate_device_id_callback = base::BindOnce(
      [](const std::string& hashed_device_id,
         base::OnceCallback<void(const std::optional<std::string>&)> callback,
         const url::Origin& security_origin, const std::string& salt) {
        content::GetMediaDeviceIDForHMAC(
            blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, salt,
            security_origin, hashed_device_id,
            content::GetUIThreadTaskRunner({}), std::move(callback));
      },
      std::move(hashed_device_id), std::move(validate_and_run_callback),
      std::move(security_origin), std::move(salt));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, std::move(translate_device_id_callback));
}

XuCameraService* g_xu_camera_service = nullptr;

}  // namespace

XuCameraService::XuCameraService(Delegate* delegate)
    : delegate_(delegate),
      service_adaptor_(mojom::XuCamera::Name_, this),
      meet_xu_guid_le_(kMeetXuGuidLe) {
  CfmHotlineClient::Get()->AddObserver(this);
}

XuCameraService::~XuCameraService() {
  CfmHotlineClient::Get()->RemoveObserver(this);
}

// static
void XuCameraService::Initialize() {
  CHECK(!g_xu_camera_service);
  g_xu_camera_service = new XuCameraService(new RealDelegate());
}

// static
void XuCameraService::InitializeForTesting(Delegate* delegate) {
  CHECK(!g_xu_camera_service);
  g_xu_camera_service = new XuCameraService(delegate);
}

// static
void XuCameraService::Shutdown() {
  CHECK(g_xu_camera_service);
  delete g_xu_camera_service;
  g_xu_camera_service = nullptr;
}

// static
XuCameraService* XuCameraService::Get() {
  return g_xu_camera_service;
}

// static
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

void XuCameraService::BindServiceContext(
    mojo::PendingReceiver<mojom::XuCamera> receiver,
    const content::GlobalRenderFrameHostId& id) {
  receivers_.Add(this, std::move(receiver), std::move(id));
}

void XuCameraService::OnBindService(
    mojo::ScopedMessagePipeHandle receiver_pipe) {
  // The Render Frame Host Id is used to identify the peripheral's device path
  // given a hased device id. If the origin of the client is not from within a
  // chromium renderer then the device paths would not be hashed.
  // We give a default RFH ID here for these cases that will fail if mistakenly
  // passed a HMAC ID.
  BindServiceContext(
      mojo::PendingReceiver<mojom::XuCamera>(std::move(receiver_pipe)),
      content::GlobalRenderFrameHostId());
}

void XuCameraService::OnAdaptorDisconnect() {
  receivers_.Clear();
}

void XuCameraService::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void XuCameraService::GetUnitId(mojom::WebcamIdPtr id,
                                const std::vector<uint8_t>& guid_le,
                                GetUnitIdCallback callback) {
  auto host_id = receivers_.current_context();

  auto get_unit_id_callback = base::BindOnce(
      &XuCameraService::GetUnitIdWithDevicePath, weak_factory_.GetWeakPtr(),
      std::move(guid_le), std::move(callback));

  GetDevicePath(std::move(id), std::move(host_id),
                std::move(get_unit_id_callback));
}

void XuCameraService::GetUnitIdWithDevicePath(
    const std::vector<uint8_t>& guid_le,
    GetUnitIdCallback callback,
    const std::optional<std::string>& dev_path) {
  if (dev_path.has_value() && IsValidDevPath(*dev_path)) {
    const bool is_ip_camera = IsIpCamera(*dev_path);
    if (is_ip_camera) {
      VLOG(4) << __func__ << ": No UnitId for IP cameras";
      std::move(callback).Run(0, 0);
      return;
    }
    auto unitId = guid_unitid_map_.find(guid_le);
    if (unitId != guid_unitid_map_.end()) {
      VLOG(4) << __func__ << ": UnitId found: "
              << static_cast<unsigned int>(unitId->second);
      std::move(callback).Run(0, unitId->second);
      return;
    }
  }

  if (!usb_manager_) {
    content::GetDeviceService().BindUsbDeviceManager(
        usb_manager_.BindNewPipeAndPassReceiver());
  }
  device::mojom::UsbEnumerationOptionsPtr options =
      device::mojom::UsbEnumerationOptions::New();
  usb_manager_->GetDevices(
      std::move(options),
      base::BindOnce(&XuCameraService::OnGetDevices, weak_factory_.GetWeakPtr(),
                     guid_le, std::move(callback)));
}

void XuCameraService::OnGetDevices(
    const std::vector<uint8_t>& guid_le,
    GetUnitIdCallback callback,
    std::vector<device::mojom::UsbDeviceInfoPtr> devices) {
  for (const auto& device_info : devices) {
    for (const auto& config : device_info->configurations) {
      for (const auto& interface : config->interfaces) {
        for (const auto& alternate : interface->alternates) {
          if (alternate->class_code == kVideoClass &&
              alternate->subclass_code == kVideoSubclass) {
            /* extra_data is additional raw data in byte form consisting of
             * an array of interfaces. */
            base::span<const uint8_t> extra(alternate->extra_data);
            while (extra.size() > kSubtypeOffset) {
              const uint8_t desc_len = extra[0];
              if (desc_len == 0) {
                break;
              }
              if (extra[kSubtypeOffset] == kXUSubtype &&
                  extra.size() >= sizeof(kXuInterface)) {
                kXuInterface curXuInterface;
                base::byte_span_from_ref(curXuInterface)
                    .copy_from(extra.first<sizeof(kXuInterface)>());
                guid_unitid_map_.insert({base::ToVector(curXuInterface.kGuidLe),
                                         curXuInterface.kUnitId});
              }
              if (desc_len > extra.size()) {
                break;
              }
              extra = extra.subspan(desc_len);
            }
          }
        }
      }
    }
  }

  auto unitId = guid_unitid_map_.find(guid_le);
  if (unitId != guid_unitid_map_.end()) {
    VLOG(4) << __func__
            << ": UnitId found: " << static_cast<unsigned int>(unitId->second);
    std::move(callback).Run(0, unitId->second);
    return;
  }

  VLOG(4) << __func__ << ": UnitId not found";
  std::move(callback).Run(ENOSYS, '0');
}

void XuCameraService::MapCtrl(mojom::WebcamIdPtr id,
                              mojom::ControlMappingPtr mapping_ctrl,
                              MapCtrlCallback callback) {
  auto host_id = receivers_.current_context();

  auto map_ctrl_callback = base::BindOnce(
      &XuCameraService::MapCtrlWithDevicePath, weak_factory_.GetWeakPtr(),
      std::move(mapping_ctrl), std::move(callback));

  GetDevicePath(std::move(id), std::move(host_id),
                std::move(map_ctrl_callback));
}

void XuCameraService::MapCtrlWithDevicePath(
    const mojom::ControlMappingPtr mapping_ctrl,
    MapCtrlCallback callback,
    const std::optional<std::string>& dev_path) const {
  uint8_t error_code = 0;
  base::ScopedFD file_descriptor;

  if (!dev_path || !IsValidV4L2DevicePath(*dev_path)) {
    LOG(ERROR) << __func__ << ": Unable to determine device path";
    std::move(callback).Run(ENOENT);
    return;
  }

  if (!mapping_ctrl) {
    LOG(ERROR) << __func__ << ": mapping_ctrl is null";
    std::move(callback).Run(EINVAL);
    return;
  }

  VLOG(4) << __func__ << ": dev_path - " << *dev_path;

  if (!delegate_->OpenFile(file_descriptor, *dev_path)) {
    LOG(ERROR) << __func__ << ": File is invalid";
    std::move(callback).Run(ENOENT);
    return;
  }

  base::FixedArray<struct uvc_menu_info> uvc_menus(
      mapping_ctrl->menu_entries ? mapping_ctrl->menu_entries->menu_info.size()
                                 : 0);

  int index = 0;
  if (mapping_ctrl->menu_entries) {
    for (const auto& menu_info : mapping_ctrl->menu_entries->menu_info) {
      if (!menu_info) {
        continue;
      }
      struct uvc_menu_info info = {
          .value = menu_info->value,
      };
      CopyTruncated(info.name, menu_info->name);
      uvc_menus[index++] = info;
    }
  }

  struct uvc_xu_control_mapping control_mapping = {
      .id = mapping_ctrl->id,
      .selector = mapping_ctrl->selector,
      .size = mapping_ctrl->size,
      .offset = mapping_ctrl->offset,
      .v4l2_type = mapping_ctrl->v4l2_type,
      .data_type = mapping_ctrl->data_type,
      .menu_info = index > 0 ? uvc_menus.data() : nullptr,
      .menu_count = static_cast<uint32_t>(index),
  };
  CopyTruncated(control_mapping.name, mapping_ctrl->name);
  CopyTruncated(control_mapping.entity, mapping_ctrl->guid);

  // Map the controls to v4l2
  int ioctl_ret =
      delegate_->Ioctl(file_descriptor, UVCIOC_CTRL_MAP, &control_mapping);
  if (ioctl_ret < 0) {
    error_code = logging::GetLastSystemErrorCode();
  } else {
    error_code = static_cast<uint8_t>(ioctl_ret);
  }

  std::move(callback).Run(error_code);
}

void XuCameraService::GetCtrl(mojom::WebcamIdPtr id,
                              mojom::CtrlTypePtr ctrl,
                              mojom::GetFn fn,
                              GetCtrlCallback callback) {
  auto host_id = receivers_.current_context();

  auto get_ctrl_callback = base::BindOnce(
      &XuCameraService::GetCtrlWithDevicePath, weak_factory_.GetWeakPtr(),
      std::move(ctrl), std::move(fn), std::move(callback));

  GetDevicePath(std::move(id), std::move(host_id),
                std::move(get_ctrl_callback));
}

void XuCameraService::GetCtrlWithDevicePath(
    const mojom::CtrlTypePtr ctrl,
    const mojom::GetFn fn,
    GetCtrlCallback callback,
    const std::optional<std::string>& dev_path) const {
  std::vector<uint8_t> data;
  if (!dev_path || !IsValidDevPath(*dev_path)) {
    LOG(ERROR) << __func__ << ": Unable to determine device path";
    std::move(callback).Run(ENOENT, data);
    return;
  }

  if (!ctrl) {
    LOG(ERROR) << __func__ << ": CtrlType is null";
    std::move(callback).Run(EINVAL, data);
    return;
  }

  VLOG(4) << __func__ << ": dev_path - " << *dev_path;

  const bool is_ip_camera = IsIpCamera(*dev_path);
  base::ScopedFD file_descriptor;
  if (!is_ip_camera && !delegate_->OpenFile(file_descriptor, *dev_path)) {
    LOG(ERROR) << __func__ << ": File is invalid";
    std::move(callback).Run(ENOENT, data);
    return;
  }

  uint8_t error_code = 0;
  // GetCtrl depending on whether id provided is WebRTC or filepath
  switch (ctrl->which()) {
    case mojom::CtrlType::Tag::kQueryCtrl: {
      auto query = std::move(ctrl->get_query_ctrl());
      if (!query) {
        LOG(ERROR) << __func__ << ": query_ctrl is null";
        std::move(callback).Run(EINVAL, data);
        return;
      }
      if (is_ip_camera) {
        GetCtrlDbus(*dev_path, std::move(query), GetRequest(fn),
                    std::move(callback));
        return;
      }
      error_code = CtrlThroughQuery(file_descriptor, std::move(query), data,
                                    GetRequest(fn));
      break;
    }
    case mojom::CtrlType::Tag::kMappingCtrl: {
      if (is_ip_camera) {
        LOG(ERROR) << __func__ << ": MappingCtrl not supported for IP cameras";
        std::move(callback).Run(ENOENT, data);
        return;
      }
      auto mapping = std::move(ctrl->get_mapping_ctrl());
      if (!mapping) {
        LOG(ERROR) << __func__ << ": mapping_ctrl is null";
        std::move(callback).Run(EINVAL, data);
        return;
      }
      error_code =
          CtrlThroughMapping(file_descriptor, std::move(mapping), data, fn);
      break;
    }
    default:
      LOG(ERROR) << __func__ << ": Invalid CtrlType::Tag";
      error_code = EINVAL;
  }
  std::move(callback).Run(error_code, data);
}

void XuCameraService::SetCtrl(mojom::WebcamIdPtr id,
                              mojom::CtrlTypePtr ctrl,
                              const std::vector<uint8_t>& data,
                              SetCtrlCallback callback) {
  auto host_id = receivers_.current_context();

  auto set_ctrl_callback = base::BindOnce(
      &XuCameraService::SetCtrlWithDevicePath, weak_factory_.GetWeakPtr(),
      std::move(ctrl), std::move(data), std::move(callback));

  GetDevicePath(std::move(id), std::move(host_id),
                std::move(set_ctrl_callback));
}

void XuCameraService::SetCtrlWithDevicePath(
    const mojom::CtrlTypePtr ctrl,
    const std::vector<uint8_t>& data,
    SetCtrlCallback callback,
    const std::optional<std::string>& dev_path) const {
  if (!dev_path || !IsValidDevPath(*dev_path)) {
    LOG(ERROR) << __func__ << ": Unable to determine device path";
    std::move(callback).Run(ENOENT);
    return;
  }

  if (!ctrl) {
    LOG(ERROR) << __func__ << ": CtrlType is null";
    std::move(callback).Run(EINVAL);
    return;
  }

  VLOG(4) << __func__ << ": dev_path - " << *dev_path;

  const bool is_ip_camera = IsIpCamera(*dev_path);
  base::ScopedFD file_descriptor;
  if (!is_ip_camera && !delegate_->OpenFile(file_descriptor, *dev_path)) {
    LOG(ERROR) << __func__ << ": File is invalid";
    std::move(callback).Run(ENOENT);
    return;
  }

  uint8_t error_code = 0;
  std::vector<uint8_t> data_(data);
  // SetCtrl depending on whether id provided is WebRTC or filepath
  switch (ctrl->which()) {
    case mojom::CtrlType::Tag::kQueryCtrl: {
      auto query = std::move(ctrl->get_query_ctrl());
      if (!query) {
        LOG(ERROR) << __func__ << ": query_ctrl is null";
        std::move(callback).Run(EINVAL);
        return;
      }
      if (is_ip_camera) {
        SetCtrlDbus(*dev_path, std::move(query), data_, std::move(callback));
        return;
      }
      error_code = CtrlThroughQuery(file_descriptor, std::move(query), data_,
                                    UVC_SET_CUR);
      break;
    }
    case mojom::CtrlType::Tag::kMappingCtrl: {
      if (is_ip_camera) {
        LOG(ERROR) << __func__ << ": MappingCtrl not supported for IP cameras";
        std::move(callback).Run(ENOENT);
        return;
      }
      mojom::ControlMappingPtr mapping = std::move(ctrl->get_mapping_ctrl());
      if (!mapping) {
        LOG(ERROR) << __func__ << ": mapping_ctrl is null";
        std::move(callback).Run(EINVAL);
        return;
      }
      int32_t newValue = 0;
      CopyFromData(&newValue, data_);
      struct v4l2_control control = {.id = mapping->id, .value = newValue};
      int ioctl_ret =
          delegate_->Ioctl(file_descriptor, VIDIOC_S_CTRL, &control);
      if (ioctl_ret < 0) {
        error_code = logging::GetLastSystemErrorCode();
      } else {
        error_code = static_cast<uint8_t>(ioctl_ret);
      }
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
                                        uint16_t size) const {
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
    logging::SystemErrorCode err = logging::GetLastSystemErrorCode();
    LOG(ERROR) << "ioctl call failed. error: "
               << logging::SystemErrorCodeToString(err);
    return err;
  }
  return error;
}

void XuCameraService::GetDevicePath(
    mojom::WebcamIdPtr id,
    const content::GlobalRenderFrameHostId& host_id,
    base::OnceCallback<void(const std::optional<std::string>&)> callback)
    const {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M160);

  if (!id) {
    LOG(ERROR) << __func__ << ": WebcamId is null";
    std::move(callback).Run(std::nullopt);
    return;
  }

  if (id->is_dev_path()) {
    const std::string& dev_path = id->get_dev_path();
    if (!IsValidDevPath(dev_path)) {
      LOG(ERROR) << __func__ << ": Invalid device path format: " << dev_path;
      std::move(callback).Run(std::nullopt);
      return;
    }
    std::move(callback).Run(dev_path);
    return;
  }

  auto hashed_device_id = id->get_device_id();

  if (!host_id || hashed_device_id.empty()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  content::RenderFrameHost* frame_host =
      content::RenderFrameHost::FromID(host_id);

  if (frame_host == nullptr) {
    VLOG(4) << __func__ << " frame_host == nullptr";
    std::move(callback).Run(std::nullopt);
    return;
  }

  content::BrowserContext* browser_context = frame_host->GetBrowserContext();

  if (browser_context == nullptr) {
    VLOG(4) << __func__ << " browser_context == nullptr";
    std::move(callback).Run(std::nullopt);
    return;
  }

  url::Origin security_origin = frame_host->GetLastCommittedOrigin();

  if (media_device_salt::MediaDeviceSaltService* salt_service =
          MediaDeviceSaltServiceFactory::GetInstance()->GetForBrowserContext(
              browser_context)) {
    salt_service->GetSalt(
        frame_host->GetStorageKey(),
        base::BindOnce(&TranslateDeviceId, hashed_device_id,
                       std::move(callback), std::move(security_origin)));
  } else {
    // If the embedder does not provide a salt service, use the browser
    // context's unique ID as salt.
    TranslateDeviceId(hashed_device_id, std::move(callback),
                      std::move(security_origin), browser_context->UniqueId());
  }
}

void XuCameraService::GetCtrlDbus(const std::string& dev_path,
                                  const mojom::ControlQueryPtr& query,
                                  const uint8_t& request,
                                  GetCtrlCallback callback) const {
  const std::string& ip_address = dev_path;
  VLOG(4) << __func__ << " ip - " << ip_address  //
          << " selector - " << static_cast<unsigned int>(query->selector)
          << " request - " << static_cast<unsigned int>(request);

  auto* ip_peripheral_service_client = IpPeripheralServiceClient::Get();
  auto get_control_callback =
      ConvertGetCtrlCallbackForDbus(std::move(callback));
  if (ip_peripheral_service_client) {
    ip_peripheral_service_client->GetControl(ip_address, meet_xu_guid_le_,
                                             query->selector, request,
                                             std::move(get_control_callback));
  } else {
    LOG(ERROR) << __func__ << " failed to get IpPeripheralServiceClient";
    std::move(get_control_callback).Run(false, std::vector<uint8_t>());
  }
}

void XuCameraService::SetCtrlDbus(const std::string& dev_path,
                                  const mojom::ControlQueryPtr& query,
                                  const std::vector<uint8_t>& data,
                                  SetCtrlCallback callback) const {
  const std::string& ip_address = dev_path;
  VLOG(4) << __func__ << " ip - " << ip_address  //
          << " selector - " << static_cast<unsigned int>(query->selector);

  auto* ip_peripheral_service_client = IpPeripheralServiceClient::Get();
  auto set_control_callback =
      ConvertSetCtrlCallbackForDbus(std::move(callback));
  if (ip_peripheral_service_client) {
    ip_peripheral_service_client->SetControl(ip_address, meet_xu_guid_le_,
                                             query->selector, data,
                                             std::move(set_control_callback));
  } else {
    LOG(ERROR) << __func__ << " failed to get IpPeripheralServiceClient";
    std::move(set_control_callback).Run(false);
  }
}

uint8_t XuCameraService::CtrlThroughQuery(const base::ScopedFD& file_descriptor,
                                          const mojom::ControlQueryPtr& query,
                                          std::vector<uint8_t>& data,
                                          const uint8_t& request) const {
  VLOG(4) << __func__ << " request - " << static_cast<unsigned int>(request)
          << " selector - " << static_cast<unsigned int>(query->selector);
  uint16_t data_len = 0;
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
    const mojom::GetFn& fn) const {
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
    int ioctl_ret = delegate_->Ioctl(file_descriptor, VIDIOC_G_CTRL, &control);
    if (ioctl_ret < 0) {
      error_code = logging::GetLastSystemErrorCode();
    } else {
      error_code = static_cast<uint8_t>(ioctl_ret);
    }
    if (error_code != 0) {
      LOG(ERROR) << __func__ << " VIDIOC_G_CTRL error_code - "
                 << static_cast<unsigned int>(error_code);
      return error_code;
    }
    CopyToData<int32_t>(&control.value, data, sizeof(control.value));
    return error_code;
  }

  struct v4l2_queryctrl query = {
      .id = mapping->id,
  };

  int ioctl_ret = delegate_->Ioctl(file_descriptor, VIDIOC_QUERYCTRL, &query);
  if (ioctl_ret < 0) {
    error_code = logging::GetLastSystemErrorCode();
  } else {
    error_code = static_cast<uint8_t>(ioctl_ret);
  }

  if (error_code != 0) {
    LOG(ERROR) << __func__ << " VIDIOC_QUERYCTRL error_code - "
               << static_cast<unsigned int>(error_code);
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
                                 size_t size) const {
  VLOG(4) << __func__ << " of size " << size;
  data.reserve(size);
  uint8_t* valueAsUint8 = reinterpret_cast<uint8_t*>(value);
  for (size_t i = 0; i < size; ++i) {
    data.push_back(*valueAsUint8);
    UNSAFE_TODO(valueAsUint8++);
  }
}

template <typename T>
void XuCameraService::CopyFromData(T* value,
                                   const std::vector<uint8_t>& data) const {
  using UnsignedT = std::make_unsigned_t<T>;
  UnsignedT unsigned_val = 0;
  size_t bytes_to_copy = std::min(data.size(), sizeof(T));
  for (size_t i = 0; i < bytes_to_copy; ++i) {
    unsigned_val |= static_cast<UnsignedT>(data[i]) << (i * 8);
  }
  *value = static_cast<T>(unsigned_val);
}

uint8_t XuCameraService::GetLength(uint8_t* data,
                                   const base::ScopedFD& file_descriptor,
                                   const uint8_t& unit_id,
                                   const uint8_t& selector) const {
  // UVC_GET_LEN is always size of 2
  uint8_t error_code =
      QueryXuControl(file_descriptor, unit_id, selector, data, UVC_GET_LEN, 2);
  VLOG_IF(4, (error_code != 0))
      << __func__
      << "query length error_code: " << static_cast<unsigned int>(error_code);
  return error_code;
}

}  // namespace ash::cfm
