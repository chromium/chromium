// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/chromevox_panel.h"

#include <memory>
#include <utility>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/utility/wm_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/shadow_types.h"

namespace ash {
namespace {

constexpr char kExtensionPrefix[] = "chrome-extension://";
constexpr char kChromeVoxMV2PanelRelativeUrl[] =
    "/chromevox/mv2/panel/panel.html";
constexpr char kChromeVoxPanelRelativeUrl[] = "/chromevox/mv3/panel/panel.html";
constexpr char kDisableSpokenFeedbackURLFragment[] = "close";
constexpr char kFocusURLFragment[] = "focus";
constexpr char kFullscreenURLFragment[] = "fullscreen";
constexpr char kWidgetName[] = "ChromeVoxPanel";
constexpr int kPanelHeight = 44;

}  // namespace

DEFINE_ELEMENT_IDENTIFIER_VALUE(kChromeVoxPanelElementId);

class ChromeVoxPanel::ChromeVoxPanelWebContentsObserver
    : public content::WebContentsObserver {
 public:
  ChromeVoxPanelWebContentsObserver(content::WebContents* web_contents,
                                    ChromeVoxPanel* panel)
      : content::WebContentsObserver(web_contents), panel_(panel) {}

  ChromeVoxPanelWebContentsObserver(const ChromeVoxPanelWebContentsObserver&) =
      delete;
  ChromeVoxPanelWebContentsObserver& operator=(
      const ChromeVoxPanelWebContentsObserver&) = delete;

  ~ChromeVoxPanelWebContentsObserver() override = default;

  // content::WebContentsObserver overrides.
  void DidFirstVisuallyNonEmptyPaint() override {
    panel_->DidFirstVisuallyNonEmptyPaint();
  }

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    // Only handle fragments for the ChromeVox extension origin.
    GURL url = web_contents()->GetLastCommittedURL();
    if (!url.SchemeIs(extensions::kExtensionScheme) ||
        url.host() != extension_misc::kChromeVoxExtensionId) {
      return;
    }

    // The ChromeVox panel uses the URL fragment to communicate state
    // to this panel host.
    std::string fragment = url.GetRef();
    if (fragment == kDisableSpokenFeedbackURLFragment) {
      AccessibilityManager::Get()->EnableSpokenFeedback(false);
    } else if (fragment == kFullscreenURLFragment) {
      panel_->EnterFullscreen();
    } else if (fragment == kFocusURLFragment) {
      panel_->Focus();
    } else {
      panel_->ExitFullscreen();
    }
  }

 private:
  raw_ptr<ChromeVoxPanel> panel_;
};

ChromeVoxPanel::ChromeVoxPanel(content::BrowserContext* browser_context) {
  SetOwnedByWidget(OwnedByWidgetPassKey());

  views::WebView* web_view = new views::WebView(browser_context);
  content::WebContents* web_contents = web_view->GetWebContents();
  web_contents_observer_ =
      std::make_unique<ChromeVoxPanelWebContentsObserver>(web_contents, this);
  web_contents->SetDelegate(this);
  if (::features::IsAccessibilityManifestV3EnabledForChromeVox()) {
    extensions::SetViewType(web_contents,
                            extensions::mojom::ViewType::kExtensionPopup);
  } else {
    extensions::SetViewType(web_contents,
                            extensions::mojom::ViewType::kComponent);
  }
  web_view->LoadInitialURL(GURL(GetUrlForContent()));
  web_view->SetProperty(views::kElementIdentifierKey, kChromeVoxPanelElementId);
  web_view_ = web_view;

  widget_ = new views::Widget();
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  // Placing the panel in the accessibility panel container allows ash to manage
  // both the window bounds and display work area.
  // The panel is only shown in the primary root window.
  ash_util::SetupWidgetInitParamsForContainerInPrimary(
      &params, ShellWindowId::kShellWindowId_AccessibilityPanelContainer);
  params.bounds = display::Screen::Get()->GetPrimaryDisplay().bounds();
  params.delegate = this;
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.name = kWidgetName;
  params.shadow_elevation = wm::kShadowElevationInactiveWindow;
  widget_->Init(std::move(params));
  // We rely on being able to set the bounds of the panel to control when it
  // captures input rather than hiding the widget because we need to continue to
  // receive key events. crbug.com/40792303.
  widget_->GetNativeWindow()->layer()->SetMasksToBounds(true);

  SetAccessibilityPanelFullscreen(false);
}

ChromeVoxPanel::~ChromeVoxPanel() = default;

void ChromeVoxPanel::CloseNow() {
  widget_->CloseNow();
}

void ChromeVoxPanel::Close() {
  // NOTE: Close the widget asynchronously because it's not legal to delete
  // a WebView/WebContents during a DidFinishNavigation callback.
  widget_->Close();
}

const views::Widget* ChromeVoxPanel::GetWidget() const {
  return widget_;
}

views::Widget* ChromeVoxPanel::GetWidget() {
  return widget_;
}

views::View* ChromeVoxPanel::GetContentsView() {
  return web_view_;
}

bool ChromeVoxPanel::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  // Eat all requests as context menus are disallowed.
  return true;
}

bool ChromeVoxPanel::ShouldAllowRendererInitiatedCrossProcessNavigation(
    bool is_outermost_main_frame_navigation) {
  // Block navigations that cause the main frame of the accessibility panel
  // to navigate to non-extension content.
  return !is_outermost_main_frame_navigation;
}

void ChromeVoxPanel::DidFirstVisuallyNonEmptyPaint() {
  widget_->Show();
}

void ChromeVoxPanel::EnterFullscreen() {
  Focus();
  SetAccessibilityPanelFullscreen(true);
}

void ChromeVoxPanel::ExitFullscreen() {
  widget_->Deactivate();
  SetCanActivate(false);
  SetAccessibilityPanelFullscreen(false);
}

void ChromeVoxPanel::Focus() {
  SetCanActivate(true);
  widget_->Activate();
  web_view_->RequestFocus();
}

void ChromeVoxPanel::SetAccessibilityPanelFullscreen(bool fullscreen) {
  gfx::Rect bounds(0, 0, 0, kPanelHeight);
  auto state = fullscreen ? AccessibilityPanelState::FULLSCREEN
                          : AccessibilityPanelState::FULL_WIDTH;
  AccessibilityController::Get()->SetAccessibilityPanelBounds(bounds, state);
}

std::string ChromeVoxPanel::GetUrlForContent() {
  std::string url(kExtensionPrefix);
  url += extension_misc::kChromeVoxExtensionId;
  if (::features::IsAccessibilityManifestV3EnabledForChromeVox()) {
    url += kChromeVoxPanelRelativeUrl;
  } else {
    url += kChromeVoxMV2PanelRelativeUrl;
  }

  return url;
}

}  // namespace ash
