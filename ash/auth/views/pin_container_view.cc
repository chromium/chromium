// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/auth/views/pin_container_view.h"

#include <memory>
#include <string>

#include "ash/auth/views/auth_input_row_view.h"
#include "ash/auth/views/pin_keyboard_input_bridge.h"
#include "ash/auth/views/pin_keyboard_view.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"

namespace ash {

namespace {

constexpr int kVerticalSeparatorInputAndKeyboardDp = 32;
constexpr int kNonEmptyWidth = 1;

}  // namespace

PinContainerView::TestApi::TestApi(PinContainerView* view) : view_(view) {}

PinContainerView::TestApi::~TestApi() = default;

raw_ptr<AuthInputRowView> PinContainerView::TestApi::GetAuthInputRowView() {
  return view_->auth_input_;
}

raw_ptr<PinKeyboardView> PinContainerView::TestApi::GetPinKeyboardView() {
  return view_->pin_keyboard_;
}

raw_ptr<PinContainerView> PinContainerView::TestApi::GetView() {
  return view_;
}

PinContainerView::PinContainerView() {
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  SetLayoutManager(std::move(layout));

  auth_input_ = AddChildView(
      std::make_unique<AuthInputRowView>(AuthInputRowView::AuthType::kPin));

  views::View* separator = AddChildView(std::make_unique<views::View>());
  separator->SetVisible(true);
  separator->SetPreferredSize(
      gfx::Size{kNonEmptyWidth, kVerticalSeparatorInputAndKeyboardDp});

  pin_keyboard_ = AddChildView(std::make_unique<PinKeyboardView>());
  bridge_ =
      std::make_unique<PinKeyboardInputBridge>(auth_input_, pin_keyboard_);
}

PinContainerView::~PinContainerView() {
  bridge_.reset();
  auth_input_ = nullptr;
  pin_keyboard_ = nullptr;
}

gfx::Size PinContainerView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const gfx::Size auth_input_size =
      auth_input_->CalculatePreferredSize(available_size);
  const gfx::Size pin_keyboard_size =
      pin_keyboard_->CalculatePreferredSize(available_size);
  const int preferred_width =
      std::max(auth_input_size.width(), pin_keyboard_size.width());
  const int preferred_height = auth_input_size.height() +
                               kVerticalSeparatorInputAndKeyboardDp +
                               pin_keyboard_size.height();
  return gfx::Size(preferred_width, preferred_height);
}

void PinContainerView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->AddState(ax::mojom::State::kInvisible);
}

std::string PinContainerView::GetObjectName() const {
  return "PinContainer";
}

void PinContainerView::RequestFocus() {
  auth_input_->RequestFocus();
}

void PinContainerView::ResetState() {
  auth_input_->ResetState();
}

void PinContainerView::SetInputEnabled(bool enabled) {
  SetEnabled(enabled);
  auth_input_->SetInputEnabled(enabled);
}

void PinContainerView::AddObserver(Observer* observer) {
  auth_input_->AddObserver(observer);
}

void PinContainerView::RemoveObserver(Observer* observer) {
  auth_input_->RemoveObserver(observer);
}

BEGIN_METADATA(PinContainerView)
END_METADATA

}  // namespace ash
