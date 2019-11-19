// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/serial/chrome_serial_delegate.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/serial/serial_chooser_context.h"
#include "chrome/browser/serial/serial_chooser_context_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_bubble_manager.h"
#include "chrome/browser/ui/permission_bubble/chooser_bubble_delegate.h"
#include "chrome/browser/ui/serial/serial_chooser.h"
#include "chrome/browser/ui/serial/serial_chooser_controller.h"
#include "content/public/browser/web_contents.h"

ChromeSerialDelegate::ChromeSerialDelegate() = default;

ChromeSerialDelegate::~ChromeSerialDelegate() = default;

std::unique_ptr<content::SerialChooser> ChromeSerialDelegate::RunChooser(
    content::RenderFrameHost* frame,
    std::vector<blink::mojom::SerialPortFilterPtr> filters,
    content::SerialChooser::Callback callback) {
  Browser* browser = chrome::FindBrowserWithWebContents(
      content::WebContents::FromRenderFrameHost(frame));
  if (!browser) {
    std::move(callback).Run(nullptr);
    return nullptr;
  }

  auto chooser_controller = std::make_unique<SerialChooserController>(
      frame, std::move(filters), std::move(callback));
  auto chooser_bubble_delegate = std::make_unique<ChooserBubbleDelegate>(
      frame, std::move(chooser_controller));
  BubbleReference bubble_reference = browser->GetBubbleManager()->ShowBubble(
      std::move(chooser_bubble_delegate));
  return std::make_unique<SerialChooser>(std::move(bubble_reference));
}

bool ChromeSerialDelegate::CanRequestPortPermission(
    content::RenderFrameHost* frame) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(frame);
  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  auto* chooser_context = SerialChooserContextFactory::GetForProfile(profile);
  return chooser_context->CanRequestObjectPermission(
      frame->GetLastCommittedOrigin(),
      web_contents->GetMainFrame()->GetLastCommittedOrigin());
}

bool ChromeSerialDelegate::HasPortPermission(
    content::RenderFrameHost* frame,
    const device::mojom::SerialPortInfo& port) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(frame);
  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  auto* chooser_context = SerialChooserContextFactory::GetForProfile(profile);
  return chooser_context->HasPortPermission(
      frame->GetLastCommittedOrigin(),
      web_contents->GetMainFrame()->GetLastCommittedOrigin(), port);
}

device::mojom::SerialPortManager* ChromeSerialDelegate::GetPortManager(
    content::RenderFrameHost* frame) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(frame);
  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  auto* chooser_context = SerialChooserContextFactory::GetForProfile(profile);
  return chooser_context->GetPortManager();
}
