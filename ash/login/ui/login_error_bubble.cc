// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_error_bubble.h"

#include "ash/login/ui/non_accessible_view.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/accessibility/view_accessibility.h"
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
  alert_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      kLockScreenAlertIcon,
      chromeos::features::IsJellyrollEnabled()
          ? static_cast<ui::ColorId>(cros_tokens::kCrosSysOnSurface)
          : kColorAshIconColorPrimary,
      kAlertIconSizeDp));

  GetViewAccessibility().SetRole(ax::mojom::Role::kAlertDialog);
}

LoginErrorBubble::~LoginErrorBubble() = default;

void LoginErrorBubble::SetContent(std::unique_ptr<views::View> content) {
  if (content_) {
    RemoveChildViewT(content_.get());
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

BEGIN_METADATA(LoginErrorBubble)
END_METADATA

}  // namespace ash
