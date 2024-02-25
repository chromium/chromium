// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/permission/media_access_permission_request.h"

#include <algorithm>
#include <utility>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/permission/aw_permission_request.h"
#include "android_webview/common/aw_features.h"
#include "content/public/browser/media_capture_devices.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

using blink::MediaStreamDevice;
using blink::MediaStreamDevices;
using content::MediaCaptureDevices;

namespace android_webview {

namespace {

// Return the device specified by |device_id| if exists, otherwise the first
// available device is returned.
const MediaStreamDevice* GetDeviceByIdOrFirstAvailable(
    const MediaStreamDevices& devices,
    const std::vector<std::string>& device_ids) {
  if (devices.empty())
    return nullptr;

  if (!device_ids.empty()) {
    for (const auto& device : devices) {
      if (device.id == device_ids.front()) {
        return &device;
      }
    }
  }

  return &devices.front();
}

}  // namespace

MediaAccessPermissionRequest::MediaAccessPermissionRequest(
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    AwPermissionManager& permission_manager,
    bool can_cache_file_url_permissions)
    : request_(request),
      callback_(std::move(callback)),
      permission_manager_(permission_manager),
      can_cache_file_url_permissions_(can_cache_file_url_permissions) {}

MediaAccessPermissionRequest::~MediaAccessPermissionRequest() {}

void MediaAccessPermissionRequest::NotifyRequestResult(bool allowed) {
  std::unique_ptr<content::MediaStreamUI> ui;
  if (!allowed) {
    permission_manager_->ClearEnumerateDevicesCachedPermission(
        request_.url_origin,
        /* remove_audio */ request_.audio_type ==
            blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
        /* remove_video */ request_.video_type ==
            blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
    std::move(callback_).Run(
        blink::mojom::StreamDevicesSet(),
        blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
        std::move(ui));
    return;
  }

  blink::mojom::StreamDevicesSet stream_devices_set;
  stream_devices_set.stream_devices.emplace_back(
      blink::mojom::StreamDevices::New());
  blink::mojom::StreamDevices& devices = *stream_devices_set.stream_devices[0];
  if (request_.audio_type ==
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE) {
    const MediaStreamDevices& audio_devices =
        audio_test_devices_.empty()
            ? MediaCaptureDevices::GetInstance()->GetAudioCaptureDevices()
            : audio_test_devices_;
    const MediaStreamDevice* device = GetDeviceByIdOrFirstAvailable(
        audio_devices, request_.requested_audio_device_ids);
    if (device)
      devices.audio_device = *device;
    if (base::FeatureList::IsEnabled(features::kWebViewEnumerateDevicesCache) &&
        (request_.url_origin.scheme() != url::kFileScheme ||
         can_cache_file_url_permissions_)) {
      permission_manager_->SetOriginCanReadEnumerateDevicesAudioLabels(
          request_.url_origin, true);
    }
  }

  if (request_.video_type ==
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE) {
    const MediaStreamDevices& video_devices =
        video_test_devices_.empty()
            ? MediaCaptureDevices::GetInstance()->GetVideoCaptureDevices()
            : video_test_devices_;
    const MediaStreamDevice* device = GetDeviceByIdOrFirstAvailable(
        video_devices, request_.requested_video_device_ids);
    if (device)
      devices.video_device = *device;
    if (base::FeatureList::IsEnabled(features::kWebViewEnumerateDevicesCache) &&
        (request_.url_origin.scheme() != url::kFileScheme ||
         can_cache_file_url_permissions_)) {
      permission_manager_->SetOriginCanReadEnumerateDevicesVideoLabels(
          request_.url_origin, true);
    }
  }

  const bool has_no_hardware =
      !devices.audio_device.has_value() && !devices.video_device.has_value();
  if (has_no_hardware) {
    stream_devices_set.stream_devices.clear();
  }
  std::move(callback_).Run(
      stream_devices_set,
      has_no_hardware ? blink::mojom::MediaStreamRequestResult::NO_HARDWARE
                      : blink::mojom::MediaStreamRequestResult::OK,
      std::move(ui));
}

const GURL& MediaAccessPermissionRequest::GetOrigin() {
  return request_.security_origin;
}

int64_t MediaAccessPermissionRequest::GetResources() {
  return (request_.audio_type ==
                  blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE
              ? AwPermissionRequest::AudioCapture
              : 0) |
         (request_.video_type ==
                  blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE
              ? AwPermissionRequest::VideoCapture
              : 0);
}

}  // namespace android_webview
