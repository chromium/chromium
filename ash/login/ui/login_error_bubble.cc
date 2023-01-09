// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_error_bubble.h"

#include "ash/login/ui/non_accessible_view.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {
namespace {

// The size of the alert icon in the error bubble.
constexpr int kAlertIconSizeDp = 20;

}  // namespace
LoginErrorBubble::LoginErrorBubble()
    : LoginErrorBubble(nullptr /*anchor_view*/) {}

LoginErrorBubble::LoginErrorBubble(base::WeakPtr<views::View> anchor_view)
    : LoginBaseBubbleView(std::move(anchor_view)) {
  alert_icon_ = AddChildView(std::make_unique<views::ImageView>());
  alert_icon_->SetPreferredSize(gfx::Size(kAlertIconSizeDp, kAlertIconSizeDp));
}

LoginErrorBubble::~LoginErrorBubble() = default;

void LoginErrorBubble::SetContent(std::unique_ptr<views::View> content) {
  if (content_) {
    RemoveChildViewT(content_);
  }
  content_ = AddChildView(std::move(content));
}

views::View* LoginErrorBubble::GetContent() {
  return content_;
}

void LoginErrorBubble::SetTextContent(const std::u16string& message) {
  message_ = message;
  SetContent(login_views_utils::CreateBubbleLabel(message, this));
}

void LoginErrorBubble::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kAlertDialog;
  node_data->SetName(GetAccessibleName());
}

void LoginErrorBubble::OnThemeChanged() {
  LoginBaseBubbleView::OnThemeChanged();
  alert_icon_->SetImage(gfx::CreateVectorIcon(
      kLockScreenAlertIcon,
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary)));
  // It is assumed that we will not have an external call to SetTextContent
  // followed by a call to SetContent (in such a case, the content would be
  // erased on theme changed and replaced with the prior message).
  if (!message_.empty()) {
    SetTextContent(message_);
  }
}

BEGIN_METADATA(LoginErrorBubble, LoginBaseBubbleView)
END_METADATA

}  // namespace ash
