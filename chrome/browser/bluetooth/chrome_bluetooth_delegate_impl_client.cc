// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bluetooth/chrome_bluetooth_delegate_impl_client.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/bluetooth/bluetooth_chooser_context_factory.h"
#include "chrome/browser/chooser_controller/title_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bluetooth/chrome_bluetooth_chooser_controller.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

#if defined(OS_ANDROID)
#include "chrome/browser/ui/android/device_dialog/chrome_bluetooth_chooser_android_delegate.h"
#include "chrome/browser/ui/android/device_dialog/chrome_bluetooth_scanning_prompt_android_delegate.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "components/permissions/android/bluetooth_chooser_android.h"
#include "components/permissions/android/bluetooth_scanning_prompt_android.h"
#else
#include "components/permissions/bluetooth_chooser_desktop.h"
#include "components/permissions/bluetooth_scanning_prompt_desktop.h"
#include "components/strings/grit/components_strings.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extensions_browser_client.h"
#endif  // OS_ANDROID

ChromeBluetoothDelegateImplClient::ChromeBluetoothDelegateImplClient() =
    default;

ChromeBluetoothDelegateImplClient::~ChromeBluetoothDelegateImplClient() =
    default;

permissions::BluetoothChooserContext*
ChromeBluetoothDelegateImplClient::GetBluetoothChooserContext(
    content::RenderFrameHost* frame) {
  auto* profile = Profile::FromBrowserContext(frame->GetBrowserContext());
  return BluetoothChooserContextFactory::GetForProfile(profile);
}

std::unique_ptr<content::BluetoothChooser>
ChromeBluetoothDelegateImplClient::RunBluetoothChooser(
    content::RenderFrameHost* frame,
    const content::BluetoothChooser::EventHandler& event_handler) {
#if defined(OS_ANDROID)
  if (vr::VrTabHelper::IsUiSuppressedInVr(
          content::WebContents::FromRenderFrameHost(frame),
          vr::UiSuppressedElement::kBluetoothChooser)) {
    return nullptr;
  }
  return std::make_unique<permissions::BluetoothChooserAndroid>(
      frame, event_handler,
      std::make_unique<ChromeBluetoothChooserAndroidDelegate>());
#else
  if (extensions::AppWindowRegistry::Get(frame->GetBrowserContext())
          ->GetAppWindowForWebContents(
              content::WebContents::FromRenderFrameHost(frame))) {
    return extensions::ExtensionsBrowserClient::Get()->CreateBluetoothChooser(
        frame, event_handler);
  }

  auto controller =
      std::make_unique<ChromeBluetoothChooserController>(frame, event_handler);
  auto controller_weak = controller->GetWeakPtr();
  return std::make_unique<permissions::BluetoothChooserDesktop>(
      std::move(controller),
      base::BindOnce(chrome::ShowDeviceChooserDialog, frame));
#endif
}

std::unique_ptr<content::BluetoothScanningPrompt>
ChromeBluetoothDelegateImplClient::ShowBluetoothScanningPrompt(
    content::RenderFrameHost* frame,
    const content::BluetoothScanningPrompt::EventHandler& event_handler) {
#if defined(OS_ANDROID)
  return std::make_unique<permissions::BluetoothScanningPromptAndroid>(
      frame, event_handler,
      std::make_unique<ChromeBluetoothScanningPromptAndroidDelegate>());
#else
  if (extensions::AppWindowRegistry::Get(frame->GetBrowserContext())
          ->GetAppWindowForWebContents(
              content::WebContents::FromRenderFrameHost(frame))) {
    return nullptr;
  }

  return std::make_unique<permissions::BluetoothScanningPromptDesktop>(
      frame, event_handler,
      CreateExtensionAwareChooserTitle(frame,
                                       IDS_BLUETOOTH_SCANNING_PROMPT_ORIGIN,
                                       IDS_BLUETOOTH_SCANNING_PROMPT_ORIGIN),
      base::BindOnce(chrome::ShowDeviceChooserDialog, frame));
#endif
}
