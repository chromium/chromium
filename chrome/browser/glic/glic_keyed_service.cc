// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_keyed_service.h"

#include "chrome/browser/glic/glic_window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/webui/glic/glic.mojom.h"
#include "content/public/browser/browser_context.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

GlicKeyedService::GlicKeyedService(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

GlicKeyedService::~GlicKeyedService() = default;

void GlicKeyedService::LaunchUI() {
  if (!window_controller_) {
    window_controller_ = std::make_unique<GlicWindowController>(
        Profile::FromBrowserContext(browser_context_));
  }
  window_controller_->Show();
}

void GlicKeyedService::CreateTab(
    const ::GURL& url,
    bool open_in_background,
    const std::optional<int32_t>& window_id,
    glic::mojom::WebClientHandler::CreateTabCallback callback) {
  // If we need to open other URL types, it should be done in a more specific
  // function.
  if (!url.SchemeIsHTTPOrHTTPS()) {
    std::move(callback).Run(nullptr);
    return;
  }
  // TODO(crbug.com/379931179): This is a placeholder implementation. Implement
  // createTab() correctly. It should consider which window to use, and observe
  // the `open_in_background` flag. It should return actual data using the
  // callback.
  NavigateParams params(Profile::FromBrowserContext(browser_context_), url,
                        ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
  std::move(callback).Run(glic::mojom::TabData::New());
}

void GlicKeyedService::ClosePanel() {
  // TODO(crbug.com/380313321)
  LOG(ERROR) << "Ignoring unimplemented ClosePanel()";
}

void GlicKeyedService::GetContextFromFocusedTab(
    bool include_inner_text,
    bool include_viewport_screenshot,
    glic::mojom::WebClientHandler::GetContextFromFocusedTabCallback callback) {
  LOG(ERROR) << "Ignoring unimplemented GetContextFromFocusedTab()";
  std::vector<uint8_t> image_data = {1, 2, 3, 4, 5, 6, 7, 8};
  std::move(callback).Run(glic::mojom::TabContextResult::New(
      glic::mojom::TabData::New(),
      glic::mojom::WebPageData::New(
          glic::mojom::DocumentData::New(url::Origin(), "test inner text")),
      glic::mojom::Screenshot::New(
          10, 10, std::move(image_data), "text/plain",
          glic::mojom::ImageOriginAnnotations::New())));
}
