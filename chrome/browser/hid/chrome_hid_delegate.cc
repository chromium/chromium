// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/chrome_hid_delegate.h"

#include <utility>

#include "chrome/browser/hid/hid_chooser_context.h"
#include "chrome/browser/hid/hid_chooser_context_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_bubble_manager.h"
#include "chrome/browser/ui/hid/hid_chooser.h"
#include "chrome/browser/ui/hid/hid_chooser_controller.h"
#include "chrome/browser/ui/permission_bubble/chooser_bubble_delegate.h"
#include "chrome/browser/usb/usb_blocklist.h"
#include "content/public/browser/web_contents.h"

ChromeHidDelegate::ChromeHidDelegate() = default;

ChromeHidDelegate::~ChromeHidDelegate() = default;

std::unique_ptr<content::HidChooser> ChromeHidDelegate::RunChooser(
    content::RenderFrameHost* frame,
    std::vector<blink::mojom::HidDeviceFilterPtr> filters,
    content::HidChooser::Callback callback) {
  Browser* browser = chrome::FindBrowserWithWebContents(
      content::WebContents::FromRenderFrameHost(frame));
  if (!browser) {
    std::move(callback).Run(nullptr);
    return nullptr;
  }

  auto chooser_controller = std::make_unique<HidChooserController>(
      frame, std::move(filters), std::move(callback));
  auto chooser_bubble_delegate = std::make_unique<ChooserBubbleDelegate>(
      frame, std::move(chooser_controller));
  BubbleReference bubble_reference = browser->GetBubbleManager()->ShowBubble(
      std::move(chooser_bubble_delegate));
  return std::make_unique<HidChooser>(std::move(bubble_reference));
}

bool ChromeHidDelegate::CanRequestDevicePermission(
    content::WebContents* web_contents,
    const url::Origin& requesting_origin) {
  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  auto* chooser_context = HidChooserContextFactory::GetForProfile(profile);
  const auto& embedding_origin =
      web_contents->GetMainFrame()->GetLastCommittedOrigin();
  return chooser_context->CanRequestObjectPermission(requesting_origin,
                                                     embedding_origin);
}

bool ChromeHidDelegate::HasDevicePermission(
    content::WebContents* web_contents,
    const url::Origin& requesting_origin,
    const device::mojom::HidDeviceInfo& device) {
  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  auto* chooser_context = HidChooserContextFactory::GetForProfile(profile);
  const auto& embedding_origin =
      web_contents->GetMainFrame()->GetLastCommittedOrigin();
  return chooser_context->HasDevicePermission(requesting_origin,
                                              embedding_origin, device);
}

device::mojom::HidManager* ChromeHidDelegate::GetHidManager(
    content::WebContents* web_contents) {
  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  auto* chooser_context = HidChooserContextFactory::GetForProfile(profile);
  return chooser_context->GetHidManager();
}
