// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/auth_panel_debug_view.h"

#include <memory>
#include <string>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/auth/views/auth_container_view.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "base/functional/bind.h"
#include "base/notimplemented.h"
#include "chromeos/ash/components/auth_panel/impl/auth_factor_store.h"
#include "chromeos/ash/components/auth_panel/impl/auth_panel.h"
#include "chromeos/ash/components/auth_panel/impl/auth_panel_event_dispatcher.h"
#include "chromeos/ash/components/auth_panel/impl/factor_auth_view_factory.h"
#include "chromeos/ash/components/osauth/public/auth_hub.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "components/account_id/account_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/vector_icons.h"

namespace ash {

AuthPanelDebugView::AuthPanelDebugView(const AccountId& account_id,
                                       bool use_legacy_authpanel) {
  //  ModalType::kSystem is used to get a semi-transparent background behind the
  //  local authentication request view, when it is used directly on a widget.
  //  The overlay consumes all the inputs from the user, so that they can only
  //  interact with the local authentication request view while it is visible.
  // SetModalType(ui::mojom::ModalType::kSystem);
  // SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  // Main view contains all other views aligned vertically and centered.

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCollapseMargins(false);

  ui::ColorId background_color_id = cros_tokens::kCrosSysSystemBaseElevated;
  SetBackground(views::CreateThemedSolidBackground(background_color_id));

  if (use_legacy_authpanel) {
    auto* auth_hub = AuthHub::Get();
    auto continuation = base::BindOnce(&AuthHub::StartAuthentication,
                                       base::Unretained(auth_hub), account_id,
                                       AuthPurpose::kLogin, this);
    auth_hub->EnsureInitialized(std::move(continuation));
  } else {
    auto* auth_panel = AddChildView(std::make_unique<AuthContainerView>(
        AuthFactorSet{AuthInputType::kPassword, AuthInputType::kPin}));
    auth_panel->SetBackground(views::CreateThemedRoundedRectBackground(
        cros_tokens::kCrosSysSystemBase, /*radius=*/8));
  }
}

AuthPanelDebugView::~AuthPanelDebugView() = default;

void AuthPanelDebugView::ChildPreferredSizeChanged(views::View* child) {
  if (GetWidget()) {
    GetWidget()->CenterWindow(
        GetWidget()->GetContentsView()->GetPreferredSize());
  }
}

void AuthPanelDebugView::OnEndAuthentication() {
  LOG(ERROR) << "AuthPanelDebugView::OnEndAuthentication";
  NOTIMPLEMENTED();
}

void AuthPanelDebugView::OnAuthPanelPreferredSizeChanged() {
  LOG(ERROR) << "AuthPanelDebugView::OnAuthPanelPreferredSizeChanged";
  NOTIMPLEMENTED();
}

void AuthPanelDebugView::OnUserAuthAttemptRejected() {
  LOG(ERROR) << "AuthPanelDebugView::OnUserAuthAttemptRejected";
  NOTIMPLEMENTED();
}

void AuthPanelDebugView::OnUserAuthAttemptConfirmed(
    AuthHubConnector* connector,
    raw_ptr<AuthFactorStatusConsumer>& out_consumer) {
  LOG(ERROR) << "AuthPanelDebugView::OnUserAuthAttemptConfirmed";
  // AddAuthPanel
  auto* auth_panel = AddChildView(std::make_unique<AuthPanel>(
      std::make_unique<FactorAuthViewFactory>(),
      std::make_unique<AuthFactorStoreFactory>(AuthHub::Get()),
      std::make_unique<AuthPanelEventDispatcherFactory>(),
      base::BindOnce(&AuthPanelDebugView::OnEndAuthentication,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&AuthPanelDebugView::OnAuthPanelPreferredSizeChanged,
                          weak_ptr_factory_.GetWeakPtr()),
      connector));
  auth_panel->SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemBase, /*radius=*/8));
  LOG(ERROR) << "auth panel visible: " << auth_panel->GetVisible();
  LOG(ERROR) << "auth panel visible bounds: "
             << auth_panel->GetVisibleBounds().ToString();
  out_consumer = auth_panel;
}

void AuthPanelDebugView::OnAccountNotFound() {
  LOG(ERROR) << "AuthPanelDebugView::OnAccountNotFound";
  NOTIMPLEMENTED();
}

void AuthPanelDebugView::OnUserAuthAttemptCancelled() {
  LOG(ERROR) << "AuthPanelDebugView::OnUserAuthAttemptCancelled";
  NOTIMPLEMENTED();
}

void AuthPanelDebugView::OnFactorAttemptFailed(AshAuthFactor factor) {
  LOG(ERROR) << "AuthPanelDebugView::OnFactorAttemptFailed";
  NOTIMPLEMENTED();
}

void AuthPanelDebugView::OnUserAuthSuccess(AshAuthFactor factor,
                                           const AuthProofToken& token) {
  LOG(ERROR) << "AuthPanelDebugView::OnUserAuthSuccess";
  NOTIMPLEMENTED();
}

}  // namespace ash
