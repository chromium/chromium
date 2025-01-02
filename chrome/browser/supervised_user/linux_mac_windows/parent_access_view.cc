// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/linux_mac_windows/parent_access_view.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

// TODO(crbug.com/383997522): Configure according to the mocks.
constexpr int kDialogWidth = 650;
constexpr int kDialogHeight = 450;

constexpr char kPacpUrl[] =
    "https://families.google.com/"
    "parentaccess/consent?callerid=5140b89c&continue=https://"
    "families.google.com";

const GURL GetPacpUrl(const GURL& blocked_url) {
  // TODO(crbug.com/383997522): Construct the url we need to
  // invoke on the PACP-side. Include any arguments that need
  // to added to the url such as the `blocked_url`.
  return GURL(kPacpUrl);
}

}  // namespace

ParentAccessView::ParentAccessView(content::BrowserContext* context) {
  CHECK(context);
  // Create the web view in the native dialog.
  web_view_ = AddChildView(std::make_unique<views::WebView>(context));
}

ParentAccessView::~ParentAccessView() = default;

// static
base::WeakPtr<ParentAccessView> ParentAccessView::ShowParentAccessDialog(
    content::WebContents* web_contents,
    const GURL& target_url) {
  CHECK(web_contents);

  auto dialog_delegate = std::make_unique<views::DialogDelegate>();
  dialog_delegate->SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  dialog_delegate->SetModalType(/*modal_type=*/ui::mojom::ModalType::kWindow);
  dialog_delegate->SetShowCloseButton(/*show_close_button=*/false);
  dialog_delegate->SetOwnedByWidget(/*delete_self=*/true);

  // Obtain the default, platform-approriate, corner radius value computed by
  // the delegate. This needs to be set in the ParentAccessView's inner web_view.
  int corner_radius = dialog_delegate->GetCornerRadius();

  auto parent_access_view =
      std::make_unique<ParentAccessView>(web_contents->GetBrowserContext());
  parent_access_view->Initialize(GetPacpUrl(target_url), corner_radius);

  // Keeps a pointer to the parent access views as it's ownership is transferred
  // to the delegate.
  auto view_weak_ptr = parent_access_view->GetWeakPtr();
  dialog_delegate->SetContentsView(std::move(parent_access_view));

  constrained_window::CreateBrowserModalDialogViews(
      std::move(dialog_delegate),
      /*parent=*/web_contents->GetTopLevelNativeWindow());

  // Shows the dialog.
  view_weak_ptr->ShowNativeView();
  return view_weak_ptr;
}

void ParentAccessView::Initialize(const GURL& pacp_url, int corner_radius) {
  // Loads the PACP widget's url. This creates the new web_contents of the
  // dialog.
  web_view_->LoadInitialURL(pacp_url);
  // Allows dismissing the dialog via the `Escape` button.
  web_view_->set_allow_accelerators(true);

  web_view_->SetPreferredSize(gfx::Size(kDialogWidth, kDialogHeight));
  web_view_->SetProperty(views::kElementIdentifierKey,
                         kLocalWebParentApprovalDialogId);

  SetUseDefaultFillLayout(true);
  corner_radius_ = corner_radius;
  is_initialized_ = true;
}

void ParentAccessView::ShowNativeView() {
  auto* widget = GetWidget();
  if (!widget) {
    return;
  }
  CHECK(is_initialized_);
  // Applies the round corners to the inner web_view.
  web_view_->holder()->SetCornerRadii(gfx::RoundedCornersF(corner_radius_));
  widget->Show();
  web_view_->RequestFocus();
}

BEGIN_METADATA(ParentAccessView)
END_METADATA
