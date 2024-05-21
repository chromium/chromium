// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_web_container_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_web_view_delegate.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/public/cpp/ash_web_view_factory.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "chromeos/ui/frame/frame_utils.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_utils.h"
#include "ui/views/window/caption_button_layout_constants.h"

namespace ash {

namespace {

// This height includes the window's |non_client_frame_view|'s height.
constexpr int kPreferredWindowHeightDip = 768;
constexpr int kPreferredWindowWidthDip = 768;

// The minimum margin of the window to the edges of the screen.
constexpr int kMinWindowMarginDip = 48;

class AssistantWebContainerClientView : public views::ClientView {
 public:
  AssistantWebContainerClientView(views::Widget* frame,
                                  AssistantWebContainerView* container)
      : views::ClientView(frame, container) {}

  AssistantWebContainerClientView(const AssistantWebContainerClientView&) =
      delete;
  AssistantWebContainerClientView& operator=(
      const AssistantWebContainerClientView&) = delete;

  ~AssistantWebContainerClientView() override = default;

  // views::ClientView:
  void UpdateWindowRoundedCorners(int corner_radius) override {
    // `NonClientFrameViewAsh` rounds the top corners of the window. The
    // client-view is responsible for rounding the bottom corners.

    DCHECK(GetWidget());

    const gfx::RoundedCornersF radii(0, 0, corner_radius, corner_radius);

    auto* container =
        views::AsViewClass<AssistantWebContainerView>(contents_view());
    container->SetBackgroundRadii(radii);

    // Match the radii of existing webview with the client view's background.
    if (AshWebView* web_view = container->web_view()) {
      web_view->SetCornerRadii(radii);
    }
  }
};

}  // namespace

AssistantWebContainerView::AssistantWebContainerView(
    AssistantWebViewDelegate* web_container_view_delegate)
    : web_container_view_delegate_(web_container_view_delegate) {
  InitLayout();
}

AssistantWebContainerView::~AssistantWebContainerView() = default;

gfx::Size AssistantWebContainerView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int non_client_frame_view_height =
      views::GetCaptionButtonLayoutSize(
          views::CaptionButtonLayoutSize::kNonBrowserCaption)
          .height();

  const gfx::Rect work_area =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(GetWidget()->GetNativeWindow())
          .work_area();

  const int width = std::min(work_area.width() - 2 * kMinWindowMarginDip,
                             kPreferredWindowWidthDip);

  const int height = std::min(work_area.height() - 2 * kMinWindowMarginDip,
                              kPreferredWindowHeightDip) -
                     non_client_frame_view_height;

  return gfx::Size(width, height);
}

void AssistantWebContainerView::ChildPreferredSizeChanged(views::View* child) {
  // Because AssistantWebContainerView has a fixed size, it does not re-layout
  // its children when their preferred size changes. To address this, we need to
  // explicitly request a layout pass.
  DeprecatedLayoutImmediately();
  SchedulePaint();
}

views::ClientView* AssistantWebContainerView::CreateClientView(
    views::Widget* widget) {
  return new AssistantWebContainerClientView(widget, this);
}

void AssistantWebContainerView::OnThemeChanged() {
  views::View::OnThemeChanged();
  UpdateBackground();
}

void AssistantWebContainerView::DidStopLoading() {
  // We should only respond to the `DidStopLoading` event the first time, to add
  // the view for contents to our view hierarchy and perform other one-time view
  // initializations.
  if (!web_view_) {
    return;
  }

  web_view_->SetPreferredSize(GetPreferredSize());
  web_view_ptr_ = AddChildView(std::move(web_view_));
  constexpr int kTopPaddingDip = 8;
  web_view_ptr_->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::TLBR(kTopPaddingDip, 0, 0, 0)));
}

void AssistantWebContainerView::DidSuppressNavigation(
    const GURL& url,
    WindowOpenDisposition disposition,
    bool from_user_gesture) {
  if (!from_user_gesture) {
    return;
  }

  // Deep links are always handled by the AssistantViewDelegate. If the
  // |disposition| indicates a desire to open a new foreground tab, we also
  // defer to the AssistantViewDelegate so that it can open the |url| in the
  // browser.
  if (assistant::util::IsDeepLinkUrl(url) ||
      disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB) {
    AssistantController::Get()->OpenUrl(url);
    return;
  }

  // Otherwise we'll allow our WebContents to navigate freely.
  web_view()->Navigate(url);
}

void AssistantWebContainerView::DidChangeCanGoBack(bool can_go_back) {
  DCHECK(web_container_view_delegate_);
  web_container_view_delegate_->UpdateBackButtonVisibility(GetWidget(),
                                                           can_go_back);
}

bool AssistantWebContainerView::GoBack() {
  return web_view() && web_view()->GoBack();
}

void AssistantWebContainerView::OpenUrl(const GURL& url) {
  RemoveContents();

  AshWebView::InitParams contents_params;
  contents_params.suppress_navigation = true;
  contents_params.minimize_on_back_key = true;

  // The webview radii needs to match the radii of the background to have
  // correct bottom rounded corners for the window.
  contents_params.rounded_corners = background_radii_;

  web_view_ = AshWebViewFactory::Get()->Create(contents_params);

  // We observe `web_view_` so that we can handle events from the
  // underlying WebContents.
  web_view()->AddObserver(this);

  // Navigate to the specified |url|.
  web_view()->Navigate(url);
}

void AssistantWebContainerView::SetBackgroundRadii(
    const gfx::RoundedCornersF& radii) {
  if (background_radii_ == radii) {
    return;
  }

  background_radii_ = radii;
  UpdateBackground();
}

void AssistantWebContainerView::SetCanGoBackForTesting(bool can_go_back) {
  DidChangeCanGoBack(can_go_back);
}

void AssistantWebContainerView::InitLayout() {
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  params.delegate = this;
  params.name = GetClassName();

  views::Widget* widget = new views::Widget;
  widget->Init(std::move(params));

  SetLayoutManager(std::make_unique<views::FillLayout>());
  UpdateBackground();
}

void AssistantWebContainerView::RemoveContents() {
  if (!web_view_ptr_) {
    return;
  }

  // Remove back button.
  web_container_view_delegate_->UpdateBackButtonVisibility(
      GetWidget(),
      /*visibility=*/false);
  RemoveChildViewT(web_view_ptr_.get())->RemoveObserver(this);
  web_view_ptr_ = nullptr;
}

void AssistantWebContainerView::UpdateBackground() {
  // Paint a theme aware background to be displayed while the web content is
  // still loading.
  const SkColor color = DarkLightModeController::Get()->IsDarkModeEnabled()
                            ? SkColorSetARGB(255, 27, 27, 27)
                            : SK_ColorWHITE;
  SetBackground(views::CreateRoundedRectBackground(color, background_radii_));
}

BEGIN_METADATA(AssistantWebContainerView)
END_METADATA

}  // namespace ash
