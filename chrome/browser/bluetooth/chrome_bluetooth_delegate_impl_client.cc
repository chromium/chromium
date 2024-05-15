// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bluetooth/chrome_bluetooth_delegate_impl_client.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/bluetooth/bluetooth_chooser_context_factory.h"
#include "chrome/browser/chooser_controller/title_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bluetooth/bluetooth_dialogs.h"
#include "chrome/browser/ui/bluetooth/chrome_bluetooth_chooser_controller.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/device_dialog/chrome_bluetooth_chooser_android_delegate.h"
#include "chrome/browser/ui/android/device_dialog/chrome_bluetooth_scanning_prompt_android_delegate.h"
#include "components/permissions/android/bluetooth_chooser_android.h"
#include "components/permissions/android/bluetooth_scanning_prompt_android.h"
#else
#include "components/permissions/bluetooth_chooser_desktop.h"
#include "components/permissions/bluetooth_scanning_prompt_desktop.h"
#include "components/strings/grit/components_strings.h"
#endif  // BUILDFLAG(IS_ANDROID)

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
#if BUILDFLAG(IS_ANDROID)
  return std::make_unique<permissions::BluetoothChooserAndroid>(
      frame, event_handler,
      std::make_unique<ChromeBluetoothChooserAndroidDelegate>(
          Profile::FromBrowserContext(frame->GetBrowserContext())));
#else
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
#if BUILDFLAG(IS_ANDROID)
  return std::make_unique<permissions::BluetoothScanningPromptAndroid>(
      frame, event_handler,
      std::make_unique<ChromeBluetoothScanningPromptAndroidDelegate>(
          Profile::FromBrowserContext(frame->GetBrowserContext())));
#else
  return std::make_unique<permissions::BluetoothScanningPromptDesktop>(
      frame, event_handler,
      CreateChooserTitle(frame, IDS_BLUETOOTH_SCANNING_PROMPT),
      base::BindOnce(chrome::ShowDeviceChooserDialog, frame));
#endif
}

void ChromeBluetoothDelegateImplClient::ShowBluetoothDevicePairDialog(
    content::RenderFrameHost* frame,
    const std::u16string& device_identifier,
    content::BluetoothDelegate::PairPromptCallback callback,
    content::BluetoothDelegate::PairingKind pairing_kind,
    const std::optional<std::u16string>& pin) {
#if PAIR_BLUETOOTH_ON_DEMAND()
  switch (pairing_kind) {
    case content::BluetoothDelegate::PairingKind::kProvidePin:
      ShowBluetoothDeviceCredentialsDialog(
          content::WebContents::FromRenderFrameHost(frame), device_identifier,
          std::move(callback));
      break;
    case content::BluetoothDelegate::PairingKind::kConfirmOnly:
    case content::BluetoothDelegate::PairingKind::kConfirmPinMatch:
      ShowBluetoothDevicePairConfirmDialog(
          content::WebContents::FromRenderFrameHost(frame), device_identifier,
          pin, std::move(callback));
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      std::move(callback).Run(content::BluetoothDelegate::PairPromptResult(
          content::BluetoothDelegate::PairPromptStatus::kCancelled));
      break;
  }
#else
  // WebBluetoothServiceImpl will only start the pairing process (which prompts
  // for credentials) on devices that pair on demand. This should never be
  // reached.
  NOTREACHED_IN_MIGRATION();
  std::move(callback).Run(content::BluetoothDelegate::PairPromptResult(
      content::BluetoothDelegate::PairPromptStatus::kCancelled));
#endif
}
