// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_web_view.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ghost_loader_view.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "components/contextual_tasks/public/features.h"
#include "components/lens/lens_overlay_dismissal_source.h"
#include "components/tabs/public/tab_interface.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/view_type_utils.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

namespace contextual_tasks {

namespace {
constexpr SkColor kDarkModeBackgroundColor = SkColorSetRGB(16, 18, 23);
}  // namespace

ContextualTasksWebView::ContextualTasksWebView(
    BrowserWindowInterface* browser_window,
    content::WebContents* toolbar_web_contents,
    content::WebContents* ghost_loader_web_contents)
    : browser_window_(browser_window),
      owns_toolbar_web_contents_(toolbar_web_contents == nullptr),
      owns_ghost_loader_web_contents_(ghost_loader_web_contents == nullptr) {
  SetProperty(views::kElementIdentifierKey,
              kContextualTasksSidePanelWebViewElementId);

  if (IsContextualTasksSidePanelRearchitectureEnabled()) {
    const bool is_dark_mode =
        contextual_tasks::ShouldUseDarkMode(browser_window->GetProfile());
    const SkColor background_color =
        is_dark_mode ? kDarkModeBackgroundColor : SK_ColorWHITE;
    SetBackground(views::CreateSolidBackground(background_color));

    views::BoxLayout* layout =
        SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical));

    toolbar_web_view_ = AddChildView(
        std::make_unique<views::WebView>(browser_window->GetProfile()));
    if (toolbar_web_contents) {
      toolbar_web_view_->SetWebContents(toolbar_web_contents);
    }
    views::WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
        toolbar_web_view_->GetWebContents(), SK_ColorTRANSPARENT);
    toolbar_web_view_->GetWebContents()->SetPageBaseBackgroundColor(
        SK_ColorTRANSPARENT);
    blink::web_pref::WebPreferences prefs =
        toolbar_web_view_->GetWebContents()->GetOrCreateWebPreferences();
    prefs.preferred_color_scheme =
        is_dark_mode ? blink::mojom::PreferredColorScheme::kDark
                     : blink::mojom::PreferredColorScheme::kLight;
    toolbar_web_view_->GetWebContents()->SetWebPreferences(prefs);

    toolbar_web_view_->SetPreferredSize(gfx::Size(0, 46));
    if (owns_toolbar_web_contents_) {
      toolbar_web_view_->LoadInitialURL(
          GURL(chrome::kChromeUIContextualTasksToolbarURL));
    }
    webui::SetBrowserWindowInterface(toolbar_web_view_->GetWebContents(),
                                     browser_window);

    auto content_container = std::make_unique<views::View>();
    content_container->SetLayoutManager(std::make_unique<views::FillLayout>());
    content_container->SetBackground(
        views::CreateSolidBackground(background_color));

    content_web_view_ = content_container->AddChildView(
        std::make_unique<views::WebView>(browser_window->GetProfile()));

    ghost_loader_view_ = content_container->AddChildView(
        std::make_unique<ContextualTasksGhostLoaderView>(
            browser_window->GetProfile(), ghost_loader_web_contents));
    views::WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
        ghost_loader_view_->GetWebContents(), background_color);
    ghost_loader_view_->GetWebContents()->SetPageBaseBackgroundColor(
        background_color);
    ghost_loader_view_->GetWebContents()->SetWebPreferences(prefs);
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
    if (owns_toolbar_web_contents_) {
      webui::SetBrowserWindowInterface(toolbar_web_view_->GetWebContents(),
                                       nullptr);
    } else {
      toolbar_web_view_->SetWebContents(nullptr);
    }
  }
  if (ghost_loader_view_ && ghost_loader_view_->web_contents()) {
    if (owns_ghost_loader_web_contents_) {
      webui::SetBrowserWindowInterface(ghost_loader_view_->GetWebContents(),
                                       nullptr);
    } else {
      ghost_loader_view_->SetWebContents(nullptr);
    }
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
    ignore_next_stop_loading_for_about_blank_ = false;
    Observe(wc);
    if (wc) {
      bool should_show_ghost_loader = false;
      if (browser_window_ && browser_window_->GetProfile()) {
        const SkColor background_color =
            contextual_tasks::ShouldUseDarkMode(browser_window_->GetProfile())
                ? kDarkModeBackgroundColor
                : SK_ColorWHITE;
        views::WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
            wc, background_color);
        wc->SetPageBaseBackgroundColor(background_color);

        auto* ui_service =
            ContextualTasksUiServiceFactory::GetForBrowserContext(
                browser_window_->GetProfile());
        if (ui_service) {
          bool is_waiting = IsTaskWaitingForUrl();
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

void ContextualTasksWebView::RequestContentViewFocus() {
  content_web_view_->RequestFocus();
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
    ignore_next_stop_loading_for_about_blank_ = true;
    return;
  }
  ignore_next_stop_loading_for_about_blank_ = false;

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
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  if (!navigation_handle->IsSameDocument()) {
    const GURL& url = navigation_handle->GetURL();
    if (url.is_empty() || url.IsAboutBlank()) {
      ignore_next_stop_loading_for_about_blank_ = true;
      return;
    }

    if (!navigation_handle->HasCommitted() ||
        navigation_handle->IsErrorPage()) {
      if (IsTaskWaitingForUrl()) {
        return;
      }
      SetGhostLoaderVisible(false);
    }
  }

  if (!navigation_handle->HasCommitted() || navigation_handle->IsErrorPage()) {
    return;
  }

  if (browser_window_ && browser_window_->GetProfile()) {
    if (auto* ui_service =
            ContextualTasksUiServiceFactory::GetForBrowserContext(
                browser_window_->GetProfile())) {
      const GURL& previous_url =
          navigation_handle->GetPreviousPrimaryMainFrameURL();
      const bool was_on_srp = ui_service->IsSearchResultsUrl(previous_url) &&
                              !ui_service->IsAiUrl(previous_url);
      if (was_on_srp && ui_service->IsAiUrl(navigation_handle->GetURL())) {
        if (auto* tab = browser_window_->GetActiveTabInterface()) {
          if (auto* controller = LensSearchController::From(tab)) {
            // TODO(crbug.com/566315172): Update to a dedicated dismissal source
            // for SRP to AIM transitions.
            controller->CloseLensAsync(lens::LensOverlayDismissalSource::
                                           kContextualTasksQuerySubmitted);
          }
        }
      }
    }
  }
}

void ContextualTasksWebView::DidFirstVisuallyNonEmptyPaint() {
  if (web_contents()) {
    const GURL& visible_url = web_contents()->GetVisibleURL();
    const GURL& committed_url = web_contents()->GetLastCommittedURL();
    if (visible_url.is_empty() || visible_url.IsAboutBlank() ||
        committed_url.IsAboutBlank()) {
      return;
    }
  }
  SetGhostLoaderVisible(false);
}

void ContextualTasksWebView::DidStopLoading() {
  if (ignore_next_stop_loading_for_about_blank_) {
    ignore_next_stop_loading_for_about_blank_ = false;
    return;
  }
  if (IsTaskWaitingForUrl()) {
    return;
  }
  if (web_contents() &&
      (web_contents()->IsLoading() ||
       web_contents()->GetLastCommittedURL().IsAboutBlank())) {
    return;
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
  if (!source || source != web_contents()) {
    return nullptr;
  }

  if (!browser_window_) {
    VLOG(1) << "Cannot find browser to open URL from tab.";
    return nullptr;
  }

  tabs::TabInterface* active_tab = browser_window_->GetActiveTabInterface();
  if (!active_tab) {
    return nullptr;
  }

  // Resolve the frame that requested this navigation.
  content::RenderFrameHost* source_rfh = nullptr;
  if (params.initiator_frame_token.has_value()) {
    source_rfh = content::RenderFrameHost::FromFrameToken(
        content::GlobalRenderFrameHostToken(
            params.initiator_process_id, params.initiator_frame_token.value()));
  } else {
    source_rfh = content::RenderFrameHost::FromID(
        params.source_render_process_id, params.source_render_frame_id);
  }
  // Only an initiator that actually lives in this side panel (including an
  // embedded <webview> guest) may be trusted; treat anything else as an unknown
  // initiator so that its state (notably user activation) is not attributed to
  // this side panel.
  if (source_rfh && content::WebContents::FromRenderFrameHost(source_rfh)
                            ->GetOutermostWebContents() != web_contents()) {
    source_rfh = nullptr;
  }

  // Restrict forwarded navigations to valid HTTP/HTTPS URLs.
  if (!params.url.is_valid() || !params.url.SchemeIsHTTPOrHTTPS()) {
    return nullptr;
  }

  content::OpenURLParams modified_params = params;

  // Sanitize `user_gesture` and `disposition` unless the request was created by
  // the browser for a context menu command. Note that IPCs sent from the legacy
  // `chrome://contextual-tasks` WebUI frame have `is_renderer_initiated`
  // cleared by `Navigator::RequestOpenURL()`, so checking
  // `!params.started_from_context_menu` ensures those IPCs are still validated.
  if (params.is_renderer_initiated || !params.started_from_context_menu) {
    if (modified_params.user_gesture &&
        (!source_rfh || !source_rfh->HasTransientUserActivation())) {
      modified_params.user_gesture = false;
    }

    switch (modified_params.disposition) {
      case WindowOpenDisposition::NEW_FOREGROUND_TAB:
      case WindowOpenDisposition::NEW_BACKGROUND_TAB:
      case WindowOpenDisposition::NEW_POPUP:
      case WindowOpenDisposition::NEW_WINDOW:
        break;
      default:
        modified_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
        break;
    }
  }

  // Reset frame_tree_node_id to prevent mismatched frame tree lookups in the
  // target navigation controller.
  modified_params.frame_tree_node_id = content::FrameTreeNodeId();

  // Pass the active tab's WebContents as the source so the navigation pipeline
  // has the correct context to evaluate disposition and popup blocking rules.
  content::WebContents* tab_contents = active_tab->GetContents();
  if (tab_contents && tab_contents->GetDelegate()) {
    return tab_contents->GetDelegate()->OpenURLFromTab(
        tab_contents, modified_params, std::move(navigation_handle_callback));
  }
  return nullptr;
}

void ContextualTasksWebView::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  // Show the native file picker for `<input type="file">` in the hosted page.
  // The default `WebContentsDelegate` implementation cancels the request.
  FileSelectHelper::RunFileChooser(render_frame_host, std::move(listener),
                                   params);
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

bool ContextualTasksWebView::IsTaskWaitingForUrl() const {
  if (!browser_window_ || !browser_window_->GetProfile() || !web_contents()) {
    return false;
  }
  auto* ui_service = ContextualTasksUiServiceFactory::GetForBrowserContext(
      browser_window_->GetProfile());
  auto* helper =
      ContextualSearchWebContentsHelper::FromWebContents(web_contents());
  return ui_service && helper && helper->task_id().has_value() &&
         ui_service->IsTaskWaitingForUrl(helper->task_id().value());
}

BEGIN_METADATA(ContextualTasksWebView)
END_METADATA

}  // namespace contextual_tasks
