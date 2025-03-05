// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/linux_mac_windows/parent_access_view.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/timer/timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/features.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/host_zoom_map.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/vector_icon_utils.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

// TODO(crbug.com/383997522): Configure according to the mocks.
constexpr int kDialogWidth = 650;
constexpr int kDialogHeight = 450;
constexpr int kMaxDialogWidth = 700;
constexpr int kMaxDialogHeight = 500;

const GURL GetPacpUrl(
    const GURL& blocked_url,
    const supervised_user::FilteringBehaviorReason& filtering_reason) {
  return supervised_user::GetParentAccessURLForDesktop(
      g_browser_process->GetApplicationLocale(), blocked_url, filtering_reason);
}

// Override the default zoom level for the parent approval dialog.
// Its size should align with native UI elements, rather than web content.
void OverrideZoomFactor(content::WebContents* web_contents,
                        const GURL& pacp_url) {
  CHECK(web_contents);
  double zoom_factor = 1;
  content::HostZoomMap* zoom_map =
      content::HostZoomMap::GetForWebContents(web_contents);
  zoom_map->SetZoomLevelForHost(pacp_url.host(),
                                blink::ZoomFactorToZoomLevel(zoom_factor));
}

}  // namespace

DialogContentLoadWithTimeoutObserver::DialogContentLoadWithTimeoutObserver(
    content::WebContents* web_contents,
    const GURL& pacp_url,
    base::OnceClosure show_view_and_destroy_timer_callback,
    base::OnceClosure cancel_flow_on_timeout_callback)
    : content::WebContentsObserver(web_contents),
      pacp_url_(pacp_url),
      show_view_and_destroy_timer_callback_(
          std::move(show_view_and_destroy_timer_callback)) {
  CHECK(show_view_and_destroy_timer_callback_);
  if (!web_contents) {
    // The web contains of the dialog were not created, abort the dialog
    // displaying.
    std::move(cancel_flow_on_timeout_callback).Run();
    return;
  }
  // Start a timer to abort the flow if the content fails to load by then.
  initial_load_timer_.Start(
      FROM_HERE,
      base::Milliseconds(
          supervised_user::kLocalWebApprovalBottomSheetLoadTimeoutMs.Get()),
      std::move(cancel_flow_on_timeout_callback));
}

DialogContentLoadWithTimeoutObserver::~DialogContentLoadWithTimeoutObserver() =
    default;

void DialogContentLoadWithTimeoutObserver::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (!render_frame_host->IsInPrimaryMainFrame() || !validated_url.is_valid() ||
      !validated_url.spec().starts_with(pacp_url_->spec())) {
    return;
  }

  // Stop the timeout timer and display the dialog.
  initial_load_timer_.Stop();
  // Causes this object to destruct.
  std::move(show_view_and_destroy_timer_callback_).Run();
}

ParentAccessView::ParentAccessView(
    content::BrowserContext* context,
    base::OnceClosure dialog_result_reset_callback)
    : dialog_result_reset_callback_(std::move(dialog_result_reset_callback)) {
  CHECK(context);
  // Create the web view in the native dialog.
  web_view_ = AddChildView(std::make_unique<views::WebView>(context));
  CHECK(web_view_);
}

ParentAccessView::~ParentAccessView() = default;

// static
base::WeakPtr<ParentAccessView> ParentAccessView::ShowParentAccessDialog(
    content::WebContents* web_contents,
    const GURL& target_url,
    const supervised_user::FilteringBehaviorReason& filtering_reason,
    WebContentsObservationCallback web_contents_observation_cb,
    base::OnceClosure abort_dialog_callback,
    base::OnceClosure dialog_result_reset_callback) {
  CHECK(web_contents);
  CHECK(web_contents_observation_cb);

  auto dialog_delegate = std::make_unique<views::DialogDelegate>();
  dialog_delegate->SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  dialog_delegate->SetModalType(/*modal_type=*/ui::mojom::ModalType::kWindow);
  // TODO(crbug.com/391629329): Until a cancellation button is provided by the PACP,
  // the dialog will offer a close "X" button.
  dialog_delegate->SetShowCloseButton(/*show_close_button=*/true);
  dialog_delegate->SetOwnedByWidget(/*delete_self=*/true);

  // Obtain the default, platform-appropriate, corner radius value computed by
  // the delegate. This needs to be set in the ParentAccessView's inner
  // web_view.
  int corner_radius = dialog_delegate->GetCornerRadius();

  auto parent_access_view = std::make_unique<ParentAccessView>(
      web_contents->GetBrowserContext(),
      std::move(dialog_result_reset_callback));
  const GURL pacp_url = GetPacpUrl(target_url, filtering_reason);
  parent_access_view->Initialize(pacp_url, corner_radius);
  // Keeps a pointer to the parent access views as it's ownership is transferred
  // to the delegate.
  auto view_weak_ptr = parent_access_view->GetWeakPtr();
  dialog_delegate->SetContentsView(std::move(parent_access_view));

  views::Widget* widget = constrained_window::CreateBrowserModalDialogViews(
      std::move(dialog_delegate),
      /*parent=*/web_contents->GetTopLevelNativeWindow());
  view_weak_ptr->widget_observations_.AddObservation(widget);

  // Starts observing the new dialog contents that have been created in
  // `Initialize`.
  std::move(web_contents_observation_cb)
      .Run(view_weak_ptr->GetWebViewContents());

  view_weak_ptr.get()->content_loader_timeout_observer_ =
      std::make_unique<DialogContentLoadWithTimeoutObserver>(
          view_weak_ptr->GetWebViewContents(), pacp_url,
          /*show_view_and_destroy_timer_callback=*/
          base::BindOnce(
              &ParentAccessView::ShowWebViewAndDestroyTimeoutObserver,
              view_weak_ptr),
          /*cancel_flow_on_timeout_callback=*/std::move(abort_dialog_callback));

  view_weak_ptr->ShowNativeView();
  return view_weak_ptr;
}

void ParentAccessView::OnWidgetClosing(views::Widget* widget) {
  if (!dialog_result_reset_callback_.is_null()) {
    std::move(dialog_result_reset_callback_).Run();
  }
  widget_observations_.RemoveAllObservations();
}

void ParentAccessView::CloseView() {
  views::Widget* widget = GetWidget();
  // TODO(crbug.com/38399752): Explore the option of owning and re-setting the
  // widget.
  if (widget) {
    widget->Close();
  }
}

void ParentAccessView::ShowWebViewAndDestroyTimeoutObserver() {
  CHECK(web_view_);
  web_view_->SetVisible(true);
  content_loader_timeout_observer_.reset();
}

void ParentAccessView::DisplayErrorMessage(content::WebContents* web_contents) {
  if (!dialog_result_reset_callback_.is_null()) {
    std::move(dialog_result_reset_callback_).Run();
  }

  views::Widget* widget = GetWidget();
  CHECK(widget);
  // Note: We cannot remove the existing "X" close button,
  // but we add buttons to dismiss the dialog.
  widget->widget_delegate()->AsDialogDelegate()->SetButtons(
      static_cast<int>(ui::mojom::DialogButton::kOk));

  // Remove the web view that displays the PACP widget content, and replace it
  // with a view that displays the error message.
  // Assume ownership of the removed view but do not destruct yet,
  // as there may be still content observers for it, which can lead to a
  // crash.
  CHECK(web_view_);
  removed_view_holder_ = RemoveChildViewT(web_view_);
  web_view_ = nullptr;
  content_loader_timeout_observer_.reset();
  CHECK(content_loader_timeout_observer_ == nullptr);

  auto error_view = std::make_unique<views::View>();
  error_view->SetProperty(views::kElementIdentifierKey,
                          kLocalWebParentApprovalDialogErrorId);

  auto layout = std::make_unique<views::BoxLayout>();
  layout->SetOrientation(views::LayoutOrientation::kVertical);
  layout->set_main_axis_alignment(views::LayoutAlignment::kCenter);
  layout->set_cross_axis_alignment(views::LayoutAlignment::kCenter);
  error_view->SetLayoutManager(std::move(layout));

  error_view->AddChildView(
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          vector_icons::kErrorIcon, ui::kColorAlertHighSeverity,
          gfx::GetDefaultSizeOfVectorIcon(vector_icons::kErrorIcon))));
  // TODO(crbug.com/394842701): Provide new appropriate strings.
  error_view->AddChildView(views::BubbleFrameView::CreateDefaultTitleLabel(
      l10n_util::GetStringUTF16(IDS_PARENT_WEBSITE_LOCAL_WEB_APPROVAL_ERROR)));

  error_view_ = AddChildView(std::move(error_view));
  widget->Show();
}

content::WebContents* ParentAccessView::GetWebViewContents() {
  CHECK(web_view_);
  CHECK(is_initialized_);
  return web_view_->web_contents();
}

void ParentAccessView::Initialize(const GURL& pacp_url, int corner_radius) {
  auto layout = std::make_unique<views::FlexLayout>();
  layout->SetOrientation(views::LayoutOrientation::kHorizontal);
  layout->SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  layout->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  SetLayoutManager(std::move(layout));

  // Loads the PACP widget's url. This creates the new web_contents of the
  // dialog.
  web_view_->LoadInitialURL(pacp_url);

  // Allows dismissing the dialog via the `Escape` button.
  web_view_->set_allow_accelerators(true);
  web_view_->SetProperty(views::kElementIdentifierKey,
                         kLocalWebParentApprovalDialogId);

  gfx::Size size = gfx::Size(kDialogWidth, kDialogHeight);
  gfx::Size maxsize = gfx::Size(kMaxDialogWidth, kMaxDialogHeight);
  web_view_->EnableSizingFromWebContents(size, maxsize);
  // TODO(crbug.com/394839768): Investigate if `SetPreferredSize` can be
  // replaced by using a layout manager.
  web_view_->SetPreferredSize(size);

  corner_radius_ = corner_radius;
  is_initialized_ = true;
  OverrideZoomFactor(GetWebViewContents(), pacp_url);
}

void ParentAccessView::ShowNativeView() {
  views::Widget* widget = GetWidget();
  if (!widget) {
    return;
  }
  CHECK(is_initialized_);
  // Applies the round corners to the inner web_view.
  web_view_->holder()->SetCornerRadii(gfx::RoundedCornersF(corner_radius_));
  // Needed to avoid flashing in dark mode while the content is loaded.
  web_view_->SetVisible(false);
  widget->Show();
  web_view_->RequestFocus();
}

BEGIN_METADATA(ParentAccessView)
END_METADATA
