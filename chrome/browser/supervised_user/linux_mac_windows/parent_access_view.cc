// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/linux_mac_windows/parent_access_view.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/timer/timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/strings/grit/components_strings.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/features.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/vector_icon_utils.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/dialog_client_view.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

constexpr int kViewWidth = 448;
constexpr int kViewHeight = 390;
constexpr int kErrorViewHeight = 376;
constexpr int kMaxWebViewHeight = 540;
constexpr gfx::Size kViewPreferredSize = gfx::Size(kViewWidth, kViewHeight);
constexpr gfx::Size kErrorViewPreferredSize =
    gfx::Size(kViewWidth, kErrorViewHeight);

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
  zoom_map->SetZoomLevelForHost(pacp_url.GetHost(),
                                blink::ZoomFactorToZoomLevel(zoom_factor));
}

bool IsEscapeEvent(const input::NativeWebKeyboardEvent& event) {
  return event.GetType() == input::NativeWebKeyboardEvent::Type::kRawKeyDown &&
         event.windows_key_code == ui::VKEY_ESCAPE;
}

}  // namespace

DialogContentLoadWithTimeoutObserver::DialogContentLoadWithTimeoutObserver(
    content::WebContents* web_contents,
    const GURL pacp_url,
    base::OnceClosure show_view_and_destroy_timer_callback,
    base::OnceClosure cancel_flow_on_timeout_callback)
    : content::WebContentsObserver(web_contents),
      pacp_url_(pacp_url),
      show_view_and_destroy_timer_callback_(
          std::move(show_view_and_destroy_timer_callback)) {
  CHECK(show_view_and_destroy_timer_callback_);
  CHECK(pacp_url_.is_valid());
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
      !validated_url.spec().starts_with(pacp_url_.spec())) {
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
  dialog_delegate->SetShowCloseButton(
      /*show_close_button=*/true);
  dialog_delegate->SetOwnedByWidget(
      views::WidgetDelegate::OwnedByWidgetPassKey());
  dialog_delegate->SetAccessibleTitle(
      l10n_util::GetStringUTF16(IDS_PARENT_WEBSITE_APPROVAL_DIALOG_A11Y_NAME));

  // Obtain the default, platform-appropriate, corner radius value computed by
  // the delegate. This needs to be set in the ParentAccessView's inner
  // web_view.
  int corner_radius = dialog_delegate->GetCornerRadius();

  auto parent_access_view = std::make_unique<ParentAccessView>(
      web_contents->GetBrowserContext(),
      std::move(dialog_result_reset_callback));
  const GURL pacp_url = GetPacpUrl(target_url, filtering_reason);
  parent_access_view->Initialize(pacp_url, corner_radius);
  // Keeps a pointer to the parent access views as its ownership is transferred
  // to the delegate.
  auto view_weak_ptr = parent_access_view->GetWeakPtr();
  dialog_delegate->SetContentsView(std::move(parent_access_view));
  views::Widget* widget = constrained_window::CreateBrowserModalDialogViews(
      std::move(dialog_delegate),
      /*parent=*/web_contents->GetTopLevelNativeWindow());
  widget->MakeCloseSynchronous(
      base::BindOnce(&ParentAccessView::OnWidgetClose, view_weak_ptr));
  view_weak_ptr->widget_observations_.AddObservation(widget);

  // Border must be set only after the widget has been created.
  view_weak_ptr->UpdateDialogBorderAndChildrenBackgroundColors();

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

void ParentAccessView::OnWidgetClose(
    views::Widget::ClosedReason /*closed_reason*/) {
  if (!dialog_result_reset_callback_.is_null()) {
    std::move(dialog_result_reset_callback_).Run();
  }
  widget_observations_.RemoveAllObservations();
  CloseView();
}

void ParentAccessView::OnWidgetThemeChanged(views::Widget*) {
  UpdateDialogBorderAndChildrenBackgroundColors();
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

void ParentAccessView::ChildPreferredSizeChanged(View* child) {
  // Adjusts the widget's size and position to fit its contents.
  // Need as the size of the web_view_ can change dynamically once its web
  // contents are loaded, so the widget size needs adjustment.
  views::Widget* widget = GetWidget();
  if (!widget) {
    return;
  }
  if (child != web_view_ && child != error_view_) {
    return;
  }
  if (!child->GetVisible()) {
    return;
  }
  // Update the widget's size.
  CHECK(widget->non_client_view());
  widget->SetSize(widget->non_client_view()->GetPreferredSize());
}

bool ParentAccessView::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  views::Widget* widget = GetWidget();
  CHECK(widget);
  if (IsEscapeEvent(event)) {
    widget->CloseWithReason(views::Widget::ClosedReason::kEscKeyPressed);
    return true;
  }
  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
      event, GetFocusManager());
}

void ParentAccessView::ResizeDueToAutoResize(content::WebContents* web_contents,
                                             const gfx::Size& new_size) {
  if (!web_view_) {
    return;
  }
  web_view_->ResizeDueToAutoResize(web_contents, new_size);
}

void ParentAccessView::DisplayErrorMessage(content::WebContents* web_contents) {
  if (!dialog_result_reset_callback_.is_null()) {
    std::move(dialog_result_reset_callback_).Run();
  }

  if (!base::FeatureList::IsEnabled(
          supervised_user::kEnableLocalWebApprovalErrorDialog)) {
    CloseView();
    return;
  }

  // Remove the web view that displays the PACP widget content, and replace it
  // with a view that displays the error message.
  // Assume ownership of the removed view but do not destruct yet,
  // as there may be still content observers for it, which can lead to a
  // crash.
  views::Widget* widget = GetWidget();
  CHECK(widget);
  CHECK(web_view_);
  removed_view_holder_ = RemoveChildViewT(web_view_);
  web_view_ = nullptr;
  content_loader_timeout_observer_.reset();
  CHECK(content_loader_timeout_observer_ == nullptr);

  auto error_view = std::make_unique<views::View>();
  error_view->SetProperty(views::kElementIdentifierKey,
                          kLocalWebParentApprovalDialogErrorId);

  const int border_inset_size = 24;
  auto layout = std::make_unique<views::BoxLayout>(
      views::LayoutOrientation::kVertical,
      /*inside_border_insets=*/
      gfx::Insets()
          .set_top_bottom(border_inset_size, border_inset_size)
          .set_left_right(border_inset_size, border_inset_size));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  auto* layout_ptr = layout.get();
  error_view->SetLayoutManager(std::move(layout));

  // Add error icon.
  auto error_icon_view =
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          vector_icons::kErrorOutlineIcon, ui::kColorAlertHighSeverity,
          gfx::GetDefaultSizeOfVectorIcon(vector_icons::kErrorOutlineIcon)));
  // Spec required the margin to be 60 px from the the top, from which we
  // subtract the additional space taken by the dialog border displaying the "X"
  // button.
  int top_margin = 60;
  int visible_border_height = widget->widget_delegate()
                                  ->AsDialogDelegate()
                                  ->GetBubbleFrameView()
                                  ->GetBorder()
                                  ->GetInsets()
                                  .height();
  top_margin -= visible_border_height;
  error_icon_view->SetProperty(views::kMarginsKey,
                               gfx::Insets().set_top(top_margin));
  error_view->AddChildView(std::move(error_icon_view));

  // Add title.
  top_margin = 16;
  auto title_label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(
          IDS_PARENTAL_LOCAL_APPROVAL_DIALOG_GENERIC_ERROR_TITLE),
      views::style::CONTEXT_DIALOG_TITLE, views::style::STYLE_PRIMARY);
  title_label->SetProperty(views::kMarginsKey,
                           gfx::Insets().set_top(top_margin));
  auto* title_label_ptr = title_label.get();
  error_view->AddChildView(std::move(title_label));

  // Add dialog body text.
  auto description_label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(
          IDS_PARENTAL_LOCAL_APPROVAL_DIALOG_GENERIC_ERROR_DESCRIPTION),
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_SECONDARY);
  description_label->SetProperty(views::kMarginsKey,
                                 gfx::Insets().set_top(top_margin));
  error_view->AddChildView(std::move(description_label));

  // Create a flexible view, to push the back button towards the bottom of the
  // view.
  auto spacer = std::make_unique<views::View>();
  views::View* spacer_ptr = spacer.get();
  error_view->AddChildView(std::move(spacer));
  layout_ptr->SetFlexForView(spacer_ptr, 1);

  // Add Back button that closes the dialog.
  auto back_button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(&ParentAccessView::CloseView, GetWeakPtr()),
      l10n_util::GetStringUTF16(
          IDS_PARENTAL_LOCAL_APPROVAL_DIALOG_GENERIC_ERROR_BACK_BUTTON));
  back_button->SetStyle(ui::ButtonStyle::kProminent);
  back_button->SetProperty(views::kElementIdentifierKey,
                           kErrorDialogBackButtonElementId);
  back_button->SetCustomPadding(
      gfx::Insets().set_top_bottom(8, 8).set_left_right(16, 16));
  // Add a row and fill it with an empty, spacer view, that pushes the
  // `Back` button to the right end.
  auto button_row = std::make_unique<views::View>();
  auto button_row_layout = std::make_unique<views::BoxLayout>();
  auto* button_row_layout_ptr = button_row_layout.get();
  button_row->SetLayoutManager(std::move(button_row_layout));
  auto button_spacer = std::make_unique<views::View>();
  views::View* button_spacer_ptr = button_spacer.get();
  button_row->AddChildView(std::move(button_spacer));
  button_row_layout_ptr->SetFlexForView(button_spacer_ptr, 1);
  button_row->AddChildView(std::move(back_button));

  error_view->AddChildView(std::move(button_row));

  error_view_ = AddChildView(std::move(error_view));
  // Triggers the dialog resizing.
  error_view_->SetPreferredSize(kErrorViewPreferredSize);

  UpdateDialogBorderAndChildrenBackgroundColors();
  widget->Show();

  if (removed_view_holder_ && removed_view_holder_->GetVisible()) {
    // Focus the error screen title to facilitate screen reader announcements in
    // the change of the dialog's content.
    GetFocusManager()->SetFocusedView(title_label_ptr);
  }
}

content::WebContents* ParentAccessView::GetWebViewContents() {
  CHECK(web_view_);
  CHECK(is_initialized_);
  return web_view_->web_contents();
}

void ParentAccessView::Initialize(const GURL& pacp_url, int corner_radius) {
  // A FillLayout is enough as the dialog only displays a child view at a time
  // (web_view_ or error_view_).
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Loads the PACP widget's url. This creates the new web_contents of the
  // dialog.
  web_view_->LoadInitialURL(pacp_url);

  // Delegate handles accelerators when the webview is focused. Also handles
  // resizing events.
  web_view_->web_contents()->SetDelegate(this);
  web_view_->SetProperty(views::kElementIdentifierKey,
                         kLocalWebParentApprovalDialogId);

  gfx::Size maxsize = gfx::Size(kViewWidth, kMaxWebViewHeight);
  web_view_->EnableSizingFromWebContents(kViewPreferredSize, maxsize);
  // TODO(crbug.com/394839768): Investigate if `SetPreferredSize` can be
  // replaced by using a layout manager.
  web_view_->SetPreferredSize(kViewPreferredSize);

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

void ParentAccessView::UpdateDialogBorderAndChildrenBackgroundColors() {
  views::View* displayed_child_view = error_view_ ? error_view_ : web_view_;
  CHECK(displayed_child_view);
  auto* widget = GetWidget();
  CHECK(widget);
  CHECK(widget->widget_delegate()->AsDialogDelegate()->GetBubbleFrameView());
  auto border = std::make_unique<views::BubbleBorder>(
      views::BubbleBorder::NONE, views::BubbleBorder::DIALOG_SHADOW);

  if (displayed_child_view == web_view_) {
    // The background color of the view needs to match the fixed webview's
    // content background.
    auto background_color = kColorParentAccessViewLocalWebApprovalBackground;
    border->SetColor(background_color);
    SetBackground(
        views::CreateRoundedRectBackground(background_color, corner_radius_));
    web_view_->SetBackground(
        views::CreateRoundedRectBackground(background_color, corner_radius_));
  } else {
    // For the error view case, remove any solid backgrounds to go with the
    // default look.
    SetBackground(nullptr);
  }

  border->set_rounded_corners(gfx::RoundedCornersF(corner_radius_));
  widget->widget_delegate()
      ->AsDialogDelegate()
      ->GetBubbleFrameView()
      ->SetBubbleBorder(std::move(border));
}

BEGIN_METADATA(ParentAccessView)
END_METADATA

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ParentAccessView,
                                      kErrorDialogBackButtonElementId);
