// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_web_view.h"

#include <algorithm>
#include <utility>

#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/ui/assistant_web_view_delegate.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/bind.h"
#include "base/callback.h"
#include "chromeos/services/assistant/public/features.h"
#include "services/content/public/cpp/navigable_contents_view.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

// AssistantWebView ------------------------------------------------------------

AssistantWebView::AssistantWebView(
    AssistantViewDelegate* assistant_view_delegate,
    AssistantWebViewDelegate* web_container_view_delegate)
    : assistant_view_delegate_(assistant_view_delegate),
      web_container_view_delegate_(web_container_view_delegate) {
  SetID(AssistantViewID::kWebView);
  InitLayout();

  assistant_view_delegate_->AddObserver(this);

  // |AssistantWebView| has its own separate container when Assistant web
  // container is enabled. The container will handle its own lifecycle.
  if (!chromeos::assistant::features::IsAssistantWebContainerEnabled())
    assistant_view_delegate_->AddUiModelObserver(this);
}

AssistantWebView::~AssistantWebView() {
  if (!chromeos::assistant::features::IsAssistantWebContainerEnabled())
    assistant_view_delegate_->RemoveUiModelObserver(this);

  assistant_view_delegate_->RemoveObserver(this);
}

const char* AssistantWebView::GetClassName() const {
  return "AssistantWebView";
}

gfx::Size AssistantWebView::CalculatePreferredSize() const {
  return gfx::Size(kPreferredWidthDip, GetHeightForWidth(kPreferredWidthDip));
}

int AssistantWebView::GetHeightForWidth(int width) const {
  // The Assistant web container has fixed height.
  if (chromeos::assistant::features::IsAssistantWebContainerEnabled())
    return INT_MAX;

  if (app_list_features::IsAssistantLauncherUIEnabled())
    return kMaxHeightEmbeddedDip;

  // |height| <= |kMaxHeightDip|.
  // |height| should not exceed the height of the usable work area.
  const gfx::Rect usable_work_area =
      assistant_view_delegate_->GetUiModel()->usable_work_area();

  return std::min(kMaxHeightDip, usable_work_area.height());
}

void AssistantWebView::ChildPreferredSizeChanged(views::View* child) {
  // Because AssistantWebView has a fixed size, it does not re-layout its
  // children when their preferred size changes. To address this, we need to
  // explicitly request a layout pass.
  Layout();
  SchedulePaint();
}

void AssistantWebView::OnFocus() {
  if (contents_)
    contents_->Focus();
}

void AssistantWebView::AboutToRequestFocusFromTabTraversal(bool reverse) {
  if (contents_)
    contents_->FocusThroughTabTraversal(reverse);
}

void AssistantWebView::InitLayout() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  // Will use ash caption buttons when Assistant web container is enabled.
  if (chromeos::assistant::features::IsAssistantWebContainerEnabled())
    return;

  // Caption bar.
  caption_bar_ = new CaptionBar();
  caption_bar_->set_delegate(this);
  caption_bar_->SetButtonVisible(AssistantButtonId::kMinimize, false);
  if (app_list_features::IsAssistantLauncherUIEnabled())
    caption_bar_->SetButtonVisible(AssistantButtonId::kClose, false);
  AddChildView(caption_bar_);
}

bool AssistantWebView::OnCaptionButtonPressed(AssistantButtonId id) {
  // We need special handling of the back button. When possible, the back button
  // should navigate backwards in the web contents' history stack.
  if (id == AssistantButtonId::kBack && contents_) {
    contents_->GoBack(base::BindOnce(
        [](const base::WeakPtr<AssistantWebView>& assistant_web_view,
           bool success) {
          // If we can't navigate back in the web contents' history stack we
          // defer back to our primary caption button delegate.
          if (!success && assistant_web_view) {
            assistant_web_view->assistant_view_delegate_
                ->GetCaptionBarDelegate()
                ->OnCaptionButtonPressed(AssistantButtonId::kBack);
          }
        },
        weak_factory_.GetWeakPtr()));
    return true;
  }

  // For all other buttons we defer to our primary caption button delegate.
  return assistant_view_delegate_->GetCaptionBarDelegate()
      ->OnCaptionButtonPressed(id);
}

void AssistantWebView::OnDeepLinkReceived(
    assistant::util::DeepLinkType type,
    const std::map<std::string, std::string>& params) {
  if (!assistant::util::IsWebDeepLinkType(type, params))
    return;

  RemoveContents();

  if (!contents_factory_.is_bound()) {
    assistant_view_delegate_->GetNavigableContentsFactoryForView(
        contents_factory_.BindNewPipeAndPassReceiver());
  }

  auto contents_params = content::mojom::NavigableContentsParams::New();
  contents_params->suppress_navigations = true;

  contents_ = std::make_unique<content::NavigableContents>(
      contents_factory_.get(), std::move(contents_params));

  // We observe |contents_| so that we can handle events from the underlying
  // web contents.
  contents_->AddObserver(this);

  // Navigate to the url associated with the received deep link.
  contents_->Navigate(assistant::util::GetWebUrl(type, params).value());
}

void AssistantWebView::DidStopLoading() {
  // We should only respond to the |DidStopLoading| event the first time, to add
  // the view for the navigable contents to our view hierarchy and perform other
  // one-time view initializations.
  if (contents_view_initialized_)
    return;

  contents_view_initialized_ = true;

  UpdateContentSize();
  AddChildView(contents_->GetView()->view());
  SetFocusBehavior(FocusBehavior::ALWAYS);

  // We need to clip the corners of our web contents to match our container.
  if (!chromeos::assistant::features::IsAssistantWebContainerEnabled()) {
    contents_->GetView()->native_view()->layer()->SetRoundedCornerRadius(
        {/*top_left=*/0, /*top_right=*/0, /*bottom_right=*/kCornerRadiusDip,
         /*bottom_left=*/kCornerRadiusDip});
  }
}

void AssistantWebView::DidSuppressNavigation(const GURL& url,
                                             WindowOpenDisposition disposition,
                                             bool from_user_gesture) {
  if (!from_user_gesture)
    return;

  // Deep links are always handled by AssistantViewDelegate. If the
  // |disposition| indicates a desire to open a new foreground tab, we also
  // defer to the AssistantViewDelegate so that it can open the |url| in the
  // browser.
  if (assistant::util::IsDeepLinkUrl(url) ||
      disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB) {
    assistant_view_delegate_->OpenUrlFromView(url);
    return;
  }

  // Otherwise we'll allow our web contents to navigate freely.
  contents_->Navigate(url);
}

void AssistantWebView::UpdateCanGoBack(bool can_go_back) {
  if (!chromeos::assistant::features::IsAssistantWebContainerEnabled())
    return;

  DCHECK(web_container_view_delegate_);
  web_container_view_delegate_->UpdateBackButtonVisibility(GetWidget(),
                                                           can_go_back);
}

void AssistantWebView::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    base::Optional<AssistantEntryPoint> entry_point,
    base::Optional<AssistantExitPoint> exit_point) {
  // When Assistant web container is enabled, |assistant_web_view| has its own
  // container and this method should not be called on it.
  DCHECK(!chromeos::assistant::features::IsAssistantWebContainerEnabled());

  // When the Assistant UI is closed we need to clear the |contents_| in order
  // to free the memory.
  if (new_visibility == AssistantVisibility::kClosed)
    RemoveContents();
}

void AssistantWebView::OnUsableWorkAreaChanged(
    const gfx::Rect& usable_work_area) {
  DCHECK(!chromeos::assistant::features::IsAssistantWebContainerEnabled());

  UpdateContentSize();
}

void AssistantWebView::RemoveContents() {
  if (!contents_)
    return;

  views::View* view = contents_->GetView()->view();
  if (view)
    RemoveChildView(view);

  SetFocusBehavior(FocusBehavior::NEVER);
  contents_->RemoveObserver(this);
  contents_.reset();
  contents_view_initialized_ = false;
}

void AssistantWebView::UpdateContentSize() {
  if (!contents_ || !contents_view_initialized_)
    return;

  if (chromeos::assistant::features::IsAssistantWebContainerEnabled()) {
    contents_->GetView()->view()->SetPreferredSize(GetPreferredSize());
    return;
  }

  const gfx::Size preferred_size = gfx::Size(
      kPreferredWidthDip, GetHeightForWidth(kPreferredWidthDip) -
                              caption_bar_->GetPreferredSize().height());
  contents_->GetView()->view()->SetPreferredSize(preferred_size);
}

}  // namespace ash
