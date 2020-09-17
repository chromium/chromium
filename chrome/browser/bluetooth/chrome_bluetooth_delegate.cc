// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bluetooth/chrome_bluetooth_delegate.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bluetooth/bluetooth_chooser_context.h"
#include "chrome/browser/bluetooth/bluetooth_chooser_context_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "third_party/blink/public/common/bluetooth/web_bluetooth_device_id.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"

using blink::WebBluetoothDeviceId;
using content::RenderFrameHost;
using content::WebContents;
using device::BluetoothUUID;

namespace {

BluetoothChooserContext* GetBluetoothChooserContext(RenderFrameHost* frame) {
  auto* profile = Profile::FromBrowserContext(frame->GetBrowserContext());
  return BluetoothChooserContextFactory::GetForProfile(profile);
}

}  // namespace

ChromeBluetoothDelegate::ChromeBluetoothDelegate() = default;

ChromeBluetoothDelegate::~ChromeBluetoothDelegate() = default;

WebBluetoothDeviceId ChromeBluetoothDelegate::GetWebBluetoothDeviceId(
    RenderFrameHost* frame,
    const std::string& device_address) {
  return GetBluetoothChooserContext(frame)->GetWebBluetoothDeviceId(
      frame->GetLastCommittedOrigin(),
      frame->GetMainFrame()->GetLastCommittedOrigin(), device_address);
}

std::string ChromeBluetoothDelegate::GetDeviceAddress(
    RenderFrameHost* frame,
    const WebBluetoothDeviceId& device_id) {
  return GetBluetoothChooserContext(frame)->GetDeviceAddress(
      frame->GetLastCommittedOrigin(),
      frame->GetMainFrame()->GetLastCommittedOrigin(), device_id);
}

WebBluetoothDeviceId ChromeBluetoothDelegate::AddScannedDevice(
    RenderFrameHost* frame,
    const std::string& device_address) {
  return GetBluetoothChooserContext(frame)->AddScannedDevice(
      frame->GetLastCommittedOrigin(),
      frame->GetMainFrame()->GetLastCommittedOrigin(), device_address);
}

WebBluetoothDeviceId ChromeBluetoothDelegate::GrantServiceAccessPermission(
    RenderFrameHost* frame,
    const device::BluetoothDevice* device,
    const blink::mojom::WebBluetoothRequestDeviceOptions* options) {
  return GetBluetoothChooserContext(frame)->GrantServiceAccessPermission(
      frame->GetLastCommittedOrigin(),
      frame->GetMainFrame()->GetLastCommittedOrigin(), device, options);
}

bool ChromeBluetoothDelegate::HasDevicePermission(
    RenderFrameHost* frame,
    const WebBluetoothDeviceId& device_id) {
  return GetBluetoothChooserContext(frame)->HasDevicePermission(
      frame->GetLastCommittedOrigin(),
      frame->GetMainFrame()->GetLastCommittedOrigin(), device_id);
}

bool ChromeBluetoothDelegate::IsAllowedToAccessService(
    RenderFrameHost* frame,
    const WebBluetoothDeviceId& device_id,
    const BluetoothUUID& service) {
  return GetBluetoothChooserContext(frame)->IsAllowedToAccessService(
      frame->GetLastCommittedOrigin(),
      frame->GetMainFrame()->GetLastCommittedOrigin(), device_id, service);
}

bool ChromeBluetoothDelegate::IsAllowedToAccessAtLeastOneService(
    RenderFrameHost* frame,
    const WebBluetoothDeviceId& device_id) {
  return GetBluetoothChooserContext(frame)->IsAllowedToAccessAtLeastOneService(
      frame->GetLastCommittedOrigin(),
      frame->GetMainFrame()->GetLastCommittedOrigin(), device_id);
}

bool ChromeBluetoothDelegate::IsAllowedToAccessManufacturerData(
    RenderFrameHost* frame,
    const WebBluetoothDeviceId& device_id,
    uint16_t manufacturer_code) {
  return GetBluetoothChooserContext(frame)->IsAllowedToAccessManufacturerData(
      frame->GetLastCommittedOrigin(),
      frame->GetMainFrame()->GetLastCommittedOrigin(), device_id,
      manufacturer_code);
}

std::vector<blink::mojom::WebBluetoothDevicePtr>
ChromeBluetoothDelegate::GetPermittedDevices(content::RenderFrameHost* frame) {
  auto* context = GetBluetoothChooserContext(frame);
  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      objects = context->GetGrantedObjects(
          frame->GetLastCommittedOrigin(),
          frame->GetMainFrame()->GetLastCommittedOrigin());
  std::vector<blink::mojom::WebBluetoothDevicePtr> permitted_devices;

  for (const auto& object : objects) {
    auto permitted_device = blink::mojom::WebBluetoothDevice::New();
    permitted_device->id =
        BluetoothChooserContext::GetObjectDeviceId(object->value);
    permitted_device->name =
        base::UTF16ToUTF8(context->GetObjectDisplayName(object->value));
    permitted_devices.push_back(std::move(permitted_device));
  }

  return permitted_devices;
}
