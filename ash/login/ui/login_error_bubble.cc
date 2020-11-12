// Copyright 2018 The Chromium Authors. All rights reserved.
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
    : LoginErrorBubble(nullptr /*content*/, nullptr /*anchor_view*/) {}

LoginErrorBubble::LoginErrorBubble(views::View* content,
                                   views::View* anchor_view)
    : LoginBaseBubbleView(anchor_view) {
  views::ImageView* alert_icon = new views::ImageView();
  alert_icon->SetPreferredSize(gfx::Size(kAlertIconSizeDp, kAlertIconSizeDp));
  alert_icon->SetImage(gfx::CreateVectorIcon(
      kLockScreenAlertIcon,
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary)));
  AddChildView(alert_icon);

  if (content) {
    content_ = content;
    AddChildView(content);
  }
}

LoginErrorBubble::~LoginErrorBubble() = default;

void LoginErrorBubble::SetContent(views::View* content) {
  if (content_)
    delete content_;
  content_ = content;
  AddChildView(content_);
}

void LoginErrorBubble::SetTextContent(const base::string16& message) {
  SetContent(login_views_utils::CreateBubbleLabel(message, this));
}

void LoginErrorBubble::SetAccessibleName(const base::string16& name) {
  accessible_name_ = name;
}

const char* LoginErrorBubble::GetClassName() const {
  return "LoginErrorBubble";
}

void LoginErrorBubble::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kAlertDialog;
  node_data->SetName(accessible_name_);
}

}  // namespace ash
