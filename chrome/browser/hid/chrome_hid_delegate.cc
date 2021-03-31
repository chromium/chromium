// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/chrome_hid_delegate.h"

#include <utility>

#include "chrome/browser/hid/hid_chooser_context.h"
#include "chrome/browser/hid/hid_chooser_context_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/hid/hid_chooser.h"
#include "chrome/browser/ui/hid/hid_chooser_controller.h"
#include "content/public/browser/web_contents.h"

namespace {

HidChooserContext* GetChooserContext(content::RenderFrameHost* frame) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(frame);
  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  return HidChooserContextFactory::GetForProfile(profile);
}

}  // namespace

ChromeHidDelegate::ChromeHidDelegate() = default;

ChromeHidDelegate::~ChromeHidDelegate() = default;

std::unique_ptr<content::HidChooser> ChromeHidDelegate::RunChooser(
    content::RenderFrameHost* frame,
    std::vector<blink::mojom::HidDeviceFilterPtr> filters,
    content::HidChooser::Callback callback) {
  auto* chooser_context = GetChooserContext(frame);
  if (!device_observer_.IsObservingSources())
    device_observer_.Add(chooser_context);
  if (!permission_observer_.IsObservingSources())
    permission_observer_.Add(chooser_context);

  return std::make_unique<HidChooser>(chrome::ShowDeviceChooserDialog(
      frame, std::make_unique<HidChooserController>(frame, std::move(filters),
                                                    std::move(callback))));
}

bool ChromeHidDelegate::CanRequestDevicePermission(
    content::WebContents* web_contents) {
  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  auto* chooser_context = HidChooserContextFactory::GetForProfile(profile);
  const auto& origin = web_contents->GetMainFrame()->GetLastCommittedOrigin();
  return chooser_context->CanRequestObjectPermission(origin);
}

bool ChromeHidDelegate::HasDevicePermission(
    content::WebContents* web_contents,
    const device::mojom::HidDeviceInfo& device) {
  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  auto* chooser_context = HidChooserContextFactory::GetForProfile(profile);
  const auto& origin = web_contents->GetMainFrame()->GetLastCommittedOrigin();
  return chooser_context->HasDevicePermission(origin, device);
}

device::mojom::HidManager* ChromeHidDelegate::GetHidManager(
    content::WebContents* web_contents) {
  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  auto* chooser_context = HidChooserContextFactory::GetForProfile(profile);
  return chooser_context->GetHidManager();
}

void ChromeHidDelegate::AddObserver(content::RenderFrameHost* frame,
                                    Observer* observer) {
  observer_list_.AddObserver(observer);
  auto* chooser_context = GetChooserContext(frame);
  if (!device_observer_.IsObservingSources())
    device_observer_.Add(chooser_context);
  if (!permission_observer_.IsObservingSources())
    permission_observer_.Add(chooser_context);
}

void ChromeHidDelegate::RemoveObserver(
    content::RenderFrameHost* frame,
    content::HidDelegate::Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

const device::mojom::HidDeviceInfo* ChromeHidDelegate::GetDeviceInfo(
    content::WebContents* web_contents,
    const std::string& guid) {
  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  auto* chooser_context = HidChooserContextFactory::GetForProfile(profile);
  return chooser_context->GetDeviceInfo(guid);
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
  device_observer_.RemoveAll();
  permission_observer_.RemoveAll();

  for (auto& observer : observer_list_)
    observer.OnHidManagerConnectionError();
}

void ChromeHidDelegate::OnHidChooserContextShutdown() {
  device_observer_.RemoveAll();
  permission_observer_.RemoveAll();
}
