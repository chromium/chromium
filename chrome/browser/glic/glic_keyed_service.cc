// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_keyed_service.h"

#include "chrome/browser/glic/glic_enabling.h"
#include "chrome/browser/glic/glic_focused_tab_manager.h"
#include "chrome/browser/glic/glic_page_context_fetcher.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/views/glic/border/border_view.h"
#include "chrome/browser/ui/webui/glic/glic.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/url_constants.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace glic {

GlicKeyedService::GlicKeyedService(content::BrowserContext* browser_context,
                                   GlicProfileManager* profile_manager)
    : browser_context_(browser_context),
      window_controller_(Profile::FromBrowserContext(browser_context)),
      focused_tab_manager_(Profile::FromBrowserContext(browser_context),
                           window_controller_),
      profile_manager_(profile_manager) {
  focused_tab_changed_subscription_ =
      focused_tab_manager_.AddFocusedTabChangedCallback(base::BindRepeating(
          &GlicKeyedService::OnFocusedTabChanged, GetWeakPtr()));
}

GlicKeyedService::~GlicKeyedService() = default;

void GlicKeyedService::LaunchUI(views::View* glic_button_view) {
  // Glic may be disabled for certain user profiles (the user is browsing in
  // incognito or guest mode, policy, etc). In those cases, the entry points to
  // this method should already have been removed.
  CHECK(GlicEnabling::IsEnabledForProfile(
      Profile::FromBrowserContext(browser_context_)));

  profile_manager_->OnUILaunching(this);
  window_controller_.Show(glic_button_view);

  auto* web_contents = GetFocusedTab();
  if (web_contents) {
    if (BorderView* border =
            BorderView::FindBorderForWebContents(web_contents)) {
      border->StartAnimation();
    }
  } else {
    // TODO(crbug.com/384740189): Find out if/how to start the border animation
    // when web_contents is not available yet.
  }
}

base::CallbackListSubscription GlicKeyedService::AddFocusedTabChangedCallback(
    FocusedTabChangedCallback callback) {
  return focused_tab_manager_.AddFocusedTabChangedCallback(callback);
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
  window_controller_.Close();
  BorderView::CancelAllAnimationsForProfile(
      Profile::FromBrowserContext(browser_context_));
}

std::optional<gfx::Size> GlicKeyedService::ResizePanel(const gfx::Size& size) {
  if (!window_controller_.Resize(size)) {
    return std::nullopt;
  }
  return window_controller_.GetSize();
}

void GlicKeyedService::SetPanelDraggableAreas(
    const std::vector<gfx::Rect>& draggable_areas) {
  window_controller_.SetDraggableAreas(draggable_areas);
}

void GlicKeyedService::GetContextFromFocusedTab(
    bool include_inner_text,
    bool include_viewport_screenshot,
    mojom::WebClientHandler::GetContextFromFocusedTabCallback callback) {
  content::WebContents* web_contents = GetFocusedTab();
  if (!web_contents) {
    // TODO(crbug.com/379773651): Clean up logspam when it's no longer useful.
    LOG(ERROR) << "GetContextFromFocusedTab: No web contents";
    std::move(callback).Run(mojom::GetContextResult::NewErrorReason(
        mojom::GetTabContextErrorReason::kWebContentsChanged));
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
             mojom::WebClientHandler::GetContextFromFocusedTabCallback callback,
             mojom::GetContextResultPtr result) {
            std::move(callback).Run(std::move(result));
          },
          std::move(fetcher), std::move(callback)));
}

void GlicKeyedService::OnFocusedTabChanged(
    const content::WebContents* focused_tab) {
  CHECK_EQ(focused_tab, GetFocusedTab());
  // TODO(crbug.com/385382048): We shouldn't cancel and restart the animation.
  // Instead we should transit the current animation state from the previous
  // browser window to the currently focused browser window.
  BorderView::CancelAllAnimationsForProfile(
      Profile::FromBrowserContext(browser_context_));
  if (focused_tab && window_controller_.HasWindow()) {
    if (BorderView* border =
            BorderView::FindBorderForWebContents(focused_tab)) {
      border->StartAnimation();
    }
  }
}

content::WebContents* GlicKeyedService::GetFocusedTab() {
  return focused_tab_manager_.GetWebContentsForFocusedTab();
}

base::WeakPtr<GlicKeyedService> GlicKeyedService::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace glic
