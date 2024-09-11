// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/serial/chrome_serial_delegate.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/serial/serial_chooser_context.h"
#include "chrome/browser/serial/serial_chooser_context_factory.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/serial/serial_chooser.h"
#include "chrome/browser/ui/serial/serial_chooser_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_GUEST_VIEW)
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#endif  // BUILDFLAG(ENABLE_GUEST_VIEW)

namespace {

SerialChooserContext* GetChooserContext(content::RenderFrameHost* frame) {
  auto* profile = Profile::FromBrowserContext(frame->GetBrowserContext());
  return SerialChooserContextFactory::GetForProfile(profile);
}

}  // namespace

ChromeSerialDelegate::ChromeSerialDelegate() = default;

ChromeSerialDelegate::~ChromeSerialDelegate() = default;

std::unique_ptr<content::SerialChooser> ChromeSerialDelegate::RunChooser(
    content::RenderFrameHost* frame,
    std::vector<blink::mojom::SerialPortFilterPtr> filters,
    std::vector<device::BluetoothUUID> allowed_bluetooth_service_class_ids,
    content::SerialChooser::Callback callback) {
  return std::make_unique<SerialChooser>(chrome::ShowDeviceChooserDialog(
      frame, std::make_unique<SerialChooserController>(
                 frame, std::move(filters),
                 std::move(allowed_bluetooth_service_class_ids),
                 std::move(callback))));
}

bool ChromeSerialDelegate::CanRequestPortPermission(
    content::RenderFrameHost* frame) {
#if BUILDFLAG(ENABLE_GUEST_VIEW)
  // <webview> and <controlledframe> can not isolate origin-based permissions
  // from the rest of profile, therefore serial is disabled inside.
  if (extensions::WebViewGuest::FromRenderFrameHost(frame)) {
    return false;
  }
#endif  // BUILDFLAG(ENABLE_GUEST_VIEW)
  return GetChooserContext(frame)->CanRequestObjectPermission(
      frame->GetMainFrame()->GetLastCommittedOrigin());
}

bool ChromeSerialDelegate::HasPortPermission(
    content::RenderFrameHost* frame,
    const device::mojom::SerialPortInfo& port) {
#if BUILDFLAG(ENABLE_GUEST_VIEW)
  // <webview> and <controlledframe> can not isolate origin-based permissions
  // from the rest of profile, therefore serial is disabled inside.
  if (extensions::WebViewGuest::FromRenderFrameHost(frame)) {
    return false;
  }
#endif  // BUILDFLAG(ENABLE_GUEST_VIEW)
  return GetChooserContext(frame)->HasPortPermission(
      frame->GetMainFrame()->GetLastCommittedOrigin(), port);
}

void ChromeSerialDelegate::RevokePortPermissionWebInitiated(
    content::RenderFrameHost* frame,
    const base::UnguessableToken& token) {
  return GetChooserContext(frame)->RevokePortPermissionWebInitiated(
      frame->GetMainFrame()->GetLastCommittedOrigin(), token);
}

const device::mojom::SerialPortInfo* ChromeSerialDelegate::GetPortInfo(
    content::RenderFrameHost* frame,
    const base::UnguessableToken& token) {
  return GetChooserContext(frame)->GetPortInfo(token);
}

device::mojom::SerialPortManager* ChromeSerialDelegate::GetPortManager(
    content::RenderFrameHost* frame) {
  return GetChooserContext(frame)->GetPortManager();
}

void ChromeSerialDelegate::AddObserver(content::RenderFrameHost* frame,
                                       Observer* observer) {
  return GetChooserContext(frame)->AddPortObserver(observer);
}

void ChromeSerialDelegate::RemoveObserver(content::RenderFrameHost* frame,
                                          Observer* observer) {
  return GetChooserContext(frame)->RemovePortObserver(observer);
}
