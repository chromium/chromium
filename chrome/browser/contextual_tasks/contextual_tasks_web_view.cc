// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_web_view.h"

#include "base/functional/callback.h"
#include "base/logging.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ghost_loader_view.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "components/contextual_tasks/public/features.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/view_type_utils.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

namespace contextual_tasks {

ContextualTasksWebView::ContextualTasksWebView(
    BrowserWindowInterface* browser_window)
    : browser_window_(browser_window) {
  SetProperty(views::kElementIdentifierKey,
              kContextualTasksSidePanelWebViewElementId);

  if (IsContextualTasksSidePanelRearchitectureEnabled()) {
    views::BoxLayout* layout =
        SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical));

    toolbar_web_view_ = AddChildView(
        std::make_unique<views::WebView>(browser_window->GetProfile()));
    views::WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
        toolbar_web_view_->GetWebContents(), SK_ColorTRANSPARENT);
    toolbar_web_view_->GetWebContents()->SetPageBaseBackgroundColor(
        SK_ColorTRANSPARENT);
    blink::web_pref::WebPreferences prefs =
        toolbar_web_view_->GetWebContents()->GetOrCreateWebPreferences();
    prefs.preferred_color_scheme =
        contextual_tasks::ShouldUseDarkMode(browser_window->GetProfile())
            ? blink::mojom::PreferredColorScheme::kDark
            : blink::mojom::PreferredColorScheme::kLight;
    toolbar_web_view_->GetWebContents()->SetWebPreferences(prefs);

    toolbar_web_view_->SetPreferredSize(gfx::Size(0, 46));
    toolbar_web_view_->LoadInitialURL(
        GURL(chrome::kChromeUIContextualTasksToolbarURL));
    webui::SetBrowserWindowInterface(toolbar_web_view_->GetWebContents(),
                                     browser_window);

    auto content_container = std::make_unique<views::View>();
    content_container->SetLayoutManager(std::make_unique<views::FillLayout>());

    content_web_view_ = content_container->AddChildView(
        std::make_unique<views::WebView>(browser_window->GetProfile()));

    ghost_loader_view_ = content_container->AddChildView(
        std::make_unique<ContextualTasksGhostLoaderView>(
            browser_window->GetProfile()));
    ghost_loader_view_->SetVisible(false);
    webui::SetBrowserWindowInterface(ghost_loader_view_->GetWebContents(),
                                     browser_window);

    auto* container_ptr = AddChildView(std::move(content_container));
    layout->SetFlexForView(container_ptr, 1);
  } else {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    content_web_view_ = AddChildView(
        std::make_unique<views::WebView>(browser_window->GetProfile()));
  }
}

ContextualTasksWebView::~ContextualTasksWebView() {
  Observe(nullptr);
  if (toolbar_web_view_ && toolbar_web_view_->web_contents()) {
    webui::SetBrowserWindowInterface(toolbar_web_view_->GetWebContents(),
                                     nullptr);
  }
  if (ghost_loader_view_ && ghost_loader_view_->web_contents()) {
    webui::SetBrowserWindowInterface(ghost_loader_view_->GetWebContents(),
                                     nullptr);
  }
  SetWebContents(nullptr);
}

base::WeakPtr<ContextualTasksWebView> ContextualTasksWebView::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ContextualTasksWebView::SetWebContents(content::WebContents* wc) {
  if (content_web_view_->web_contents() == wc) {
    return;
  }

  if (content_web_view_->web_contents()) {
    if (!content_web_view_->web_contents()->IsBeingDestroyed()) {
      content_web_view_->web_contents()->WasHidden();
    }
    content_web_view_->web_contents()->SetDelegate(nullptr);
  }
  DetachWebContentsModalDialogManager(content_web_view_->web_contents());

  AttachWebContentsModalDialogManager(wc);
  content_web_view_->SetWebContents(wc);

  if (IsContextualTasksSidePanelRearchitectureEnabled()) {
    Observe(wc);
    if (wc) {
      bool should_show_ghost_loader = false;
      if (browser_window_ && browser_window_->GetProfile()) {
        auto* ui_service =
            ContextualTasksUiServiceFactory::GetForBrowserContext(
                browser_window_->GetProfile());
        if (ui_service) {
          auto* helper = ContextualSearchWebContentsHelper::FromWebContents(wc);
          bool is_waiting =
              helper && helper->task_id().has_value() &&
              ui_service->IsTaskWaitingForUrl(helper->task_id().value());
          bool is_loading = wc->IsLoading();
          const GURL& url = wc->GetVisibleURL();
          bool is_ai = ui_service->IsAiUrl(url);
          if (is_waiting) {
            should_show_ghost_loader = true;
          } else if (is_loading) {
            if (!url.is_empty() && !url.IsAboutBlank() && !is_ai) {
              should_show_ghost_loader = true;
            }
          }
        }
      }
      SetGhostLoaderVisible(should_show_ghost_loader);
    } else {
      SetGhostLoaderVisible(false);
    }
  }

  if (wc) {
    wc->WasShown();
    wc->SetDelegate(this);
    extensions::SetViewType(wc, extensions::mojom::ViewType::kComponent);
  }
}

content::WebContents* ContextualTasksWebView::web_contents() const {
  return content_web_view_ ? content_web_view_->web_contents() : nullptr;
}

void ContextualTasksWebView::SetGhostLoaderVisible(bool visible) {
  if (!ghost_loader_view_) {
    return;
  }
  if (visible && !GetIsGhostLoaderEnabled()) {
    return;
  }
  ghost_loader_view_->SetVisible(visible);
}

bool ContextualTasksWebView::IsGhostLoaderVisible() const {
  return ghost_loader_view_ && ghost_loader_view_->GetVisible();
}

void ContextualTasksWebView::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  const GURL& url = navigation_handle->GetURL();
  if (url.is_empty() || url.IsAboutBlank()) {
    return;
  }

  bool is_ai_url = false;
  if (browser_window_ && browser_window_->GetProfile()) {
    auto* ui_service = ContextualTasksUiServiceFactory::GetForBrowserContext(
        browser_window_->GetProfile());
    if (ui_service && ui_service->IsAiUrl(url)) {
      is_ai_url = true;
    }
  }

  if (is_ai_url) {
    SetGhostLoaderVisible(false);
  } else {
    SetGhostLoaderVisible(true);
  }
}

void ContextualTasksWebView::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  const GURL& url = navigation_handle->GetURL();
  if (url.is_empty() || url.IsAboutBlank()) {
    return;
  }

  bool is_ai_url = false;
  if (browser_window_ && browser_window_->GetProfile()) {
    auto* ui_service = ContextualTasksUiServiceFactory::GetForBrowserContext(
        browser_window_->GetProfile());
    if (ui_service && ui_service->IsAiUrl(url)) {
      is_ai_url = true;
    }
  }

  if (is_ai_url) {
    SetGhostLoaderVisible(false);
  } else {
    SetGhostLoaderVisible(true);
  }
}

void ContextualTasksWebView::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  if (!navigation_handle->HasCommitted() || navigation_handle->IsErrorPage()) {
    SetGhostLoaderVisible(false);
  }
}

void ContextualTasksWebView::DidFirstVisuallyNonEmptyPaint() {
  SetGhostLoaderVisible(false);
}

void ContextualTasksWebView::DidStopLoading() {
  if (browser_window_ && browser_window_->GetProfile() && web_contents()) {
    auto* ui_service = ContextualTasksUiServiceFactory::GetForBrowserContext(
        browser_window_->GetProfile());
    auto* helper =
        ContextualSearchWebContentsHelper::FromWebContents(web_contents());
    if (ui_service && helper && helper->task_id().has_value() &&
        ui_service->IsTaskWaitingForUrl(helper->task_id().value())) {
      return;
    }
  }
  SetGhostLoaderVisible(false);
}

void ContextualTasksWebView::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  // Handle the media access requests for voice search by routing them through
  // `MediaCaptureDevicesDispatcher`.
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, std::move(callback), /*extension=*/nullptr);
}

bool ContextualTasksWebView::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
      event, GetFocusManager());
}

content::WebContents* ContextualTasksWebView::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  BrowserWindowInterface* browser = webui::GetBrowserWindowInterface(source);
  if (browser) {
    return browser->OpenURL(params, std::move(navigation_handle_callback));
  } else {
    VLOG(1) << "Cannot find browser to open URL from tab.";
    return nullptr;
  }
}

web_modal::WebContentsModalDialogHost*
ContextualTasksWebView::GetWebContentsModalDialogHost(
    content::WebContents* web_contents) {
  BrowserWindowInterface* browser =
      webui::GetBrowserWindowInterface(web_contents);
  if (!browser) {
    return nullptr;
  }
  return browser->GetWebContentsModalDialogHostForWindow();
}

void ContextualTasksWebView::AttachWebContentsModalDialogManager(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return;
  }
  web_modal::WebContentsModalDialogManager::CreateForWebContents(web_contents);
  web_modal::WebContentsModalDialogManager::FromWebContents(web_contents)
      ->SetDelegate(this);
}

void ContextualTasksWebView::DetachWebContentsModalDialogManager(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return;
  }
  auto* dialog_manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  if (dialog_manager) {
    dialog_manager->SetDelegate(nullptr);
  }
}

BEGIN_METADATA(ContextualTasksWebView)
END_METADATA

}  // namespace contextual_tasks
