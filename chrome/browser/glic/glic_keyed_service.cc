// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_keyed_service.h"

#include "chrome/browser/glic/glic_page_context_fetcher.h"
#include "chrome/browser/glic/glic_window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/webui/glic/glic.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/url_constants.h"
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
  content::WebContents* web_contents = GetWebContentsForContext();
  if (!web_contents) {
    std::move(callback).Run(nullptr);
    return;
  }

  auto fetcher = std::make_unique<glic::GlicPageContextFetcher>();
  fetcher->Fetch(
      web_contents, include_inner_text, include_viewport_screenshot,
      base::BindOnce(
          // Bind `fetcher` to the callback to keep it in scope until it
          // returns.
          // TODO(harringtond): Consider adding throttling of how often we fetch
          // context.
          // TODO(harringtond): Consider deleting the fetcher if the page
          // handler is unbound before the fetch completes.
          [](std::unique_ptr<glic::GlicPageContextFetcher> fetcher,
             glic::mojom::WebClientHandler::GetContextFromFocusedTabCallback
                 callback,
             glic::mojom::TabContextResultPtr tab_context_result) {
            std::move(callback).Run(std::move(tab_context_result));
          },
          std::move(fetcher), std::move(callback)));
}

content::WebContents* GlicKeyedService::GetWebContentsForContext() {
  // TODO(harringtond): This is a placeholder implementation.
  // It returns the active tab, unless that tab is a chrome: tab,
  // in which case it returns the next tab or the previous tab if the next tab
  // doesn't exist.
  Browser* last_active_browser = BrowserList::GetInstance()->GetLastActive();
  if (!last_active_browser) {
    return nullptr;
  }
  TabStripModel* tab_strip_model = last_active_browser->GetTabStripModel();
  const int index_options[3] = {tab_strip_model->active_index(),
                                tab_strip_model->active_index() + 1,
                                tab_strip_model->active_index() - 1};

  for (int index : index_options) {
    if (index < tab_strip_model->count()) {
      content::WebContents* web_contents =
          tab_strip_model->GetWebContentsAt(index);
      if (web_contents &&
          !web_contents->GetURL().SchemeIs(content::kChromeUIScheme)) {
        return web_contents;
      }
    }
  }
  return nullptr;
}
