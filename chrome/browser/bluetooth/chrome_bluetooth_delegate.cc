// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bluetooth/chrome_bluetooth_delegate.h"

#include <memory>

#include "base/scoped_observer.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/bluetooth/bluetooth_chooser_context.h"
#include "chrome/browser/bluetooth/bluetooth_chooser_context_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "third_party/blink/public/common/bluetooth/web_bluetooth_device_id.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"

#if defined(OS_ANDROID)
#include "chrome/browser/ui/android/device_dialog/bluetooth_chooser_android.h"
#include "chrome/browser/ui/android/device_dialog/bluetooth_scanning_prompt_android.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#else
#include "chrome/browser/ui/bluetooth/bluetooth_chooser_controller.h"
#include "chrome/browser/ui/bluetooth/bluetooth_chooser_desktop.h"
#include "chrome/browser/ui/bluetooth/bluetooth_scanning_prompt_controller.h"
#include "chrome/browser/ui/bluetooth/bluetooth_scanning_prompt_desktop.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extensions_browser_client.h"
#endif  // OS_ANDROID

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

std::unique_ptr<content::BluetoothChooser>
ChromeBluetoothDelegate::RunBluetoothChooser(
    content::RenderFrameHost* frame,
    const content::BluetoothChooser::EventHandler& event_handler) {
#if defined(OS_ANDROID)
  if (vr::VrTabHelper::IsUiSuppressedInVr(
          WebContents::FromRenderFrameHost(frame),
          vr::UiSuppressedElement::kBluetoothChooser)) {
    return nullptr;
  }
  return std::make_unique<BluetoothChooserAndroid>(frame, event_handler);
#else
  if (extensions::AppWindowRegistry::Get(frame->GetBrowserContext())
          ->GetAppWindowForWebContents(
              WebContents::FromRenderFrameHost(frame))) {
    return extensions::ExtensionsBrowserClient::Get()->CreateBluetoothChooser(
        frame, event_handler);
  }

  return std::make_unique<BluetoothChooserDesktop>(frame, event_handler);
#endif
}

std::unique_ptr<content::BluetoothScanningPrompt>
ChromeBluetoothDelegate::ShowBluetoothScanningPrompt(
    content::RenderFrameHost* frame,
    const content::BluetoothScanningPrompt::EventHandler& event_handler) {
#if defined(OS_ANDROID)
  return std::make_unique<BluetoothScanningPromptAndroid>(frame, event_handler);
#else
  if (extensions::AppWindowRegistry::Get(frame->GetBrowserContext())
          ->GetAppWindowForWebContents(
              WebContents::FromRenderFrameHost(frame))) {
    return nullptr;
  }

  return std::make_unique<BluetoothScanningPromptDesktop>(frame, event_handler);
#endif
}

WebBluetoothDeviceId ChromeBluetoothDelegate::GetWebBluetoothDeviceId(
    RenderFrameHost* frame,
    const std::string& device_address) {
  return GetBluetoothChooserContext(frame)->GetWebBluetoothDeviceId(
      frame->GetMainFrame()->GetLastCommittedOrigin(), device_address);
}

std::string ChromeBluetoothDelegate::GetDeviceAddress(
    RenderFrameHost* frame,
    const WebBluetoothDeviceId& device_id) {
  return GetBluetoothChooserContext(frame)->GetDeviceAddress(
      frame->GetMainFrame()->GetLastCommittedOrigin(), device_id);
}

WebBluetoothDeviceId ChromeBluetoothDelegate::AddScannedDevice(
    RenderFrameHost* frame,
    const std::string& device_address) {
  return GetBluetoothChooserContext(frame)->AddScannedDevice(
      frame->GetMainFrame()->GetLastCommittedOrigin(), device_address);
}

WebBluetoothDeviceId ChromeBluetoothDelegate::GrantServiceAccessPermission(
    RenderFrameHost* frame,
    const device::BluetoothDevice* device,
    const blink::mojom::WebBluetoothRequestDeviceOptions* options) {
  return GetBluetoothChooserContext(frame)->GrantServiceAccessPermission(
      frame->GetMainFrame()->GetLastCommittedOrigin(), device, options);
}

bool ChromeBluetoothDelegate::HasDevicePermission(
    RenderFrameHost* frame,
    const WebBluetoothDeviceId& device_id) {
  return GetBluetoothChooserContext(frame)->HasDevicePermission(
      frame->GetMainFrame()->GetLastCommittedOrigin(), device_id);
}

bool ChromeBluetoothDelegate::IsAllowedToAccessService(
    RenderFrameHost* frame,
    const WebBluetoothDeviceId& device_id,
    const BluetoothUUID& service) {
  return GetBluetoothChooserContext(frame)->IsAllowedToAccessService(
      frame->GetMainFrame()->GetLastCommittedOrigin(), device_id, service);
}

bool ChromeBluetoothDelegate::IsAllowedToAccessAtLeastOneService(
    RenderFrameHost* frame,
    const WebBluetoothDeviceId& device_id) {
  return GetBluetoothChooserContext(frame)->IsAllowedToAccessAtLeastOneService(
      frame->GetMainFrame()->GetLastCommittedOrigin(), device_id);
}

bool ChromeBluetoothDelegate::IsAllowedToAccessManufacturerData(
    RenderFrameHost* frame,
    const WebBluetoothDeviceId& device_id,
    uint16_t manufacturer_code) {
  return GetBluetoothChooserContext(frame)->IsAllowedToAccessManufacturerData(
      frame->GetMainFrame()->GetLastCommittedOrigin(), device_id,
      manufacturer_code);
}

void ChromeBluetoothDelegate::AddFramePermissionObserver(
    FramePermissionObserver* observer) {
  std::unique_ptr<ChooserContextPermissionObserver>& chooser_observer =
      chooser_observers_[observer->GetRenderFrameHost()];
  if (!chooser_observer) {
    chooser_observer = std::make_unique<ChooserContextPermissionObserver>(
        this, GetBluetoothChooserContext(observer->GetRenderFrameHost()));
  }

  chooser_observer->AddFramePermissionObserver(observer);
}

void ChromeBluetoothDelegate::RemoveFramePermissionObserver(
    FramePermissionObserver* observer) {
  auto it = chooser_observers_.find(observer->GetRenderFrameHost());
  if (it == chooser_observers_.end())
    return;
  it->second->RemoveFramePermissionObserver(observer);
}

std::vector<blink::mojom::WebBluetoothDevicePtr>
ChromeBluetoothDelegate::GetPermittedDevices(content::RenderFrameHost* frame) {
  auto* context = GetBluetoothChooserContext(frame);
  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      objects = context->GetGrantedObjects(
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

ChromeBluetoothDelegate::ChooserContextPermissionObserver::
    ChooserContextPermissionObserver(ChromeBluetoothDelegate* owning_delegate,
                                     permissions::ChooserContextBase* context)
    : owning_delegate_(owning_delegate) {
  observer_.Observe(context);
}

ChromeBluetoothDelegate::ChooserContextPermissionObserver::
    ~ChooserContextPermissionObserver() = default;

void ChromeBluetoothDelegate::ChooserContextPermissionObserver::
    OnPermissionRevoked(const url::Origin& origin) {
  observers_pending_removal_.clear();
  is_traversing_observers_ = true;

  for (auto& observer : observer_list_)
    observer.OnPermissionRevoked(origin);

  is_traversing_observers_ = false;
  for (FramePermissionObserver* observer : observers_pending_removal_)
    RemoveFramePermissionObserver(observer);
}

void ChromeBluetoothDelegate::ChooserContextPermissionObserver::
    AddFramePermissionObserver(FramePermissionObserver* observer) {
  observer_list_.AddObserver(observer);
}

void ChromeBluetoothDelegate::ChooserContextPermissionObserver::
    RemoveFramePermissionObserver(FramePermissionObserver* observer) {
  if (is_traversing_observers_) {
    observers_pending_removal_.emplace_back(observer);
    return;
  }

  observer_list_.RemoveObserver(observer);
  if (observer_list_.empty())
    owning_delegate_->chooser_observers_.erase(observer->GetRenderFrameHost());
  // Previous call destructed this instance. Don't add code after this.
}
