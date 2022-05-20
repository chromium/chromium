// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/chrome_hid_delegate.h"

#include <utility>

#include "base/observer_list.h"
#include "chrome/browser/hid/hid_chooser_context.h"
#include "chrome/browser/hid/hid_chooser_context_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/hid/hid_chooser.h"
#include "chrome/browser/ui/hid/hid_chooser_controller.h"
#include "content/public/browser/render_frame_host.h"

namespace {

HidChooserContext* GetChooserContext(content::BrowserContext* browser_context) {
  auto* profile = Profile::FromBrowserContext(browser_context);
  return HidChooserContextFactory::GetForProfile(profile);
}

}  // namespace

ChromeHidDelegate::ChromeHidDelegate() = default;

ChromeHidDelegate::~ChromeHidDelegate() = default;

std::unique_ptr<content::HidChooser> ChromeHidDelegate::RunChooser(
    content::RenderFrameHost* render_frame_host,
    std::vector<blink::mojom::HidDeviceFilterPtr> filters,
    std::vector<blink::mojom::HidDeviceFilterPtr> exclusion_filters,
    content::HidChooser::Callback callback) {
  DCHECK(render_frame_host);
  auto* chooser_context =
      GetChooserContext(render_frame_host->GetBrowserContext());
  if (!device_observation_.IsObserving())
    device_observation_.Observe(chooser_context);
  if (!permission_observation_.IsObserving())
    permission_observation_.Observe(chooser_context);

  return std::make_unique<HidChooser>(chrome::ShowDeviceChooserDialog(
      render_frame_host,
      std::make_unique<HidChooserController>(
          render_frame_host, std::move(filters), std::move(exclusion_filters),
          std::move(callback))));
}

bool ChromeHidDelegate::CanRequestDevicePermission(
    content::BrowserContext* browser_context,
    const url::Origin& origin) {
  return GetChooserContext(browser_context)->CanRequestObjectPermission(origin);
}

bool ChromeHidDelegate::HasDevicePermission(
    content::BrowserContext* browser_context,
    const url::Origin& origin,
    const device::mojom::HidDeviceInfo& device) {
  return GetChooserContext(browser_context)
      ->HasDevicePermission(origin, device);
}

void ChromeHidDelegate::RevokeDevicePermission(
    content::BrowserContext* browser_context,
    const url::Origin& origin,
    const device::mojom::HidDeviceInfo& device) {
  return GetChooserContext(browser_context)
      ->RevokeDevicePermission(origin, device);
}

device::mojom::HidManager* ChromeHidDelegate::GetHidManager(
    content::BrowserContext* browser_context) {
  return GetChooserContext(browser_context)->GetHidManager();
}

void ChromeHidDelegate::AddObserver(content::BrowserContext* browser_context,
                                    Observer* observer) {
  observer_list_.AddObserver(observer);
  auto* chooser_context = GetChooserContext(browser_context);
  if (!device_observation_.IsObserving())
    device_observation_.Observe(chooser_context);
  if (!permission_observation_.IsObserving())
    permission_observation_.Observe(chooser_context);
}

void ChromeHidDelegate::RemoveObserver(
    content::HidDelegate::Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

const device::mojom::HidDeviceInfo* ChromeHidDelegate::GetDeviceInfo(
    content::BrowserContext* browser_context,
    const std::string& guid) {
  auto* chooser_context = GetChooserContext(browser_context);
  return chooser_context->GetDeviceInfo(guid);
}

bool ChromeHidDelegate::IsFidoAllowedForOrigin(
    content::BrowserContext* browser_context,
    const url::Origin& origin) {
  auto* chooser_context = GetChooserContext(browser_context);
  return chooser_context->IsFidoAllowedForOrigin(origin);
}

void ChromeHidDelegate::OnPermissionRevoked(const url::Origin& origin) {
  for (auto& observer : observer_list_)
    observer.OnPermissionRevoked(origin);
}

void ChromeHidDelegate::OnDeviceAdded(
    const device::mojom::HidDeviceInfo& device_info) {
  for (auto& observer : observer_list_)
    observer.OnDeviceAdded(device_info);
}

void ChromeHidDelegate::OnDeviceRemoved(
    const device::mojom::HidDeviceInfo& device_info) {
  for (auto& observer : observer_list_)
    observer.OnDeviceRemoved(device_info);
}

void ChromeHidDelegate::OnDeviceChanged(
    const device::mojom::HidDeviceInfo& device_info) {
  for (auto& observer : observer_list_)
    observer.OnDeviceChanged(device_info);
}

void ChromeHidDelegate::OnHidManagerConnectionError() {
  device_observation_.Reset();
  permission_observation_.Reset();

  for (auto& observer : observer_list_)
    observer.OnHidManagerConnectionError();
}

void ChromeHidDelegate::OnHidChooserContextShutdown() {
  device_observation_.Reset();
  permission_observation_.Reset();
}
