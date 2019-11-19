// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tab_capture/offscreen_tabs_owner.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/stl_util.h"
#include "chrome/browser/extensions/api/tab_capture/tab_capture_registry.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"

using content::WebContents;

namespace {

// Upper limit on the number of simultaneous off-screen tabs per extension
// instance.
const int kMaxOffscreenTabsPerExtension = 4;

}  // namespace

namespace extensions {

OffscreenTabsOwner::OffscreenTabsOwner(WebContents* extension_web_contents)
    : extension_web_contents_(extension_web_contents) {
  DCHECK(extension_web_contents_);
}

OffscreenTabsOwner::~OffscreenTabsOwner() {}

// static
OffscreenTabsOwner* OffscreenTabsOwner::Get(
    content::WebContents* extension_web_contents) {
  // CreateForWebContents() really means "create if not exists."
  CreateForWebContents(extension_web_contents);
  return FromWebContents(extension_web_contents);
}

OffscreenTab* OffscreenTabsOwner::OpenNewTab(
    const GURL& start_url,
    const gfx::Size& initial_size,
    const std::string& optional_presentation_id) {
  if (tabs_.size() >= kMaxOffscreenTabsPerExtension)
    return nullptr;  // Maximum number of offscreen tabs reached.

  tabs_.emplace_back(std::make_unique<OffscreenTab>(
      this, extension_web_contents_->GetBrowserContext()));
  tabs_.back()->Start(start_url, initial_size, optional_presentation_id);
  return tabs_.back().get();
}

void OffscreenTabsOwner::RequestMediaAccessPermission(
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  // This method is being called to check whether an extension is permitted to
  // capture the page.  Verify that the request is being made by the extension
  // that spawned this OffscreenTab.

  // Find the extension ID associated with the extension background page's
  // WebContents.
  content::BrowserContext* const extension_browser_context =
      extension_web_contents_->GetBrowserContext();
  const extensions::Extension* const extension =
      ProcessManager::Get(extension_browser_context)
          ->GetExtensionForWebContents(extension_web_contents_);
  const std::string extension_id = extension ? extension->id() : "";
  LOG_IF(DFATAL, extension_id.empty())
      << "Extension that started this OffscreenTab was not found.";

  // If verified, allow any tab capture audio/video devices that were requested.
  extensions::TabCaptureRegistry* const tab_capture_registry =
      extensions::TabCaptureRegistry::Get(extension_browser_context);
  blink::MediaStreamDevices devices;
  if (tab_capture_registry &&
      tab_capture_registry->VerifyRequest(
          request.render_process_id, request.render_frame_id, extension_id)) {
    if (request.audio_type ==
        blink::mojom::MediaStreamType::GUM_TAB_AUDIO_CAPTURE) {
      devices.push_back(blink::MediaStreamDevice(
          blink::mojom::MediaStreamType::GUM_TAB_AUDIO_CAPTURE, std::string(),
          std::string()));
    }
    if (request.video_type ==
        blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE) {
      devices.push_back(blink::MediaStreamDevice(
          blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE, std::string(),
          std::string()));
    }
  }

  DVLOG(2) << "Allowing " << devices.size()
           << " capture devices for OffscreenTab content.";

  std::move(callback).Run(
      devices,
      devices.empty() ? blink::mojom::MediaStreamRequestResult::INVALID_STATE
                      : blink::mojom::MediaStreamRequestResult::OK,
      nullptr);
}

void OffscreenTabsOwner::DestroyTab(OffscreenTab* tab) {
  for (auto iter = tabs_.begin(); iter != tabs_.end(); ++iter) {
    if (iter->get() == tab) {
      tabs_.erase(iter);
      break;
    }
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(OffscreenTabsOwner)

}  // namespace extensions
