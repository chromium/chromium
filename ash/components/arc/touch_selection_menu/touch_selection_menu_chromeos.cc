// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/touch_selection_menu/touch_selection_menu_chromeos.h"

#include <utility>

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/touch_selection_menu/touch_selection_menu_runner_chromeos.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/utility/wm_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/models/image_model.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/touch_selection/touch_selection_metrics.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"

namespace {

constexpr size_t kSmallIconSizeInDip = 16;

}  // namespace

TouchSelectionMenuChromeOS::TouchSelectionMenuChromeOS(
    views::TouchSelectionMenuRunnerViews* owner,
    base::WeakPtr<ui::TouchSelectionMenuClient> client,
    aura::Window* context,
    arc::mojom::TextSelectionActionPtr action)
    : views::TouchSelectionMenuViews(owner, client, context),
      action_(std::move(action)),
      display_id_(
          display::Screen::GetScreen()->GetDisplayNearestWindow(context).id()) {
}

void TouchSelectionMenuChromeOS::SetActionsForTesting(
    std::vector<arc::mojom::TextSelectionActionPtr> actions) {
  action_ = std::move(actions[0]);

  // Since we are forcing new button entries here, it is very likely that the
  // default action buttons are already added, we should remove the existent
  // buttons if any, and then call CreateButtons, this will call the parent
  // method too.
  RemoveAllChildViews();

  CreateButtons();
}

void TouchSelectionMenuChromeOS::CreateButtons() {
  if (action_) {
    views::LabelButton* button = CreateButton(
        base::UTF8ToUTF16(action_->title),
        base::BindRepeating(&TouchSelectionMenuChromeOS::ActionButtonPressed,
                            base::Unretained(this)));
    if (action_->bitmap_icon) {
      gfx::ImageSkia original(
          gfx::ImageSkia::CreateFrom1xBitmap(action_->bitmap_icon.value()));
      gfx::ImageSkia icon = gfx::ImageSkiaOperations::CreateResizedImage(
          original, skia::ImageOperations::RESIZE_BEST,
          gfx::Size(kSmallIconSizeInDip, kSmallIconSizeInDip));
      button->SetImageModel(views::Button::ButtonState::STATE_NORMAL,
                            ui::ImageModel::FromImageSkia(icon));
    }
    CreateSeparator();
  }

  views::TouchSelectionMenuViews::CreateButtons();
}

void TouchSelectionMenuChromeOS::OnBeforeBubbleWidgetInit(
    views::Widget::InitParams* params,
    views::Widget* widget) const {
  ash_util::SetupWidgetInitParamsForContainer(
      params, ash::kShellWindowId_SettingBubbleContainer);
}

TouchSelectionMenuChromeOS::~TouchSelectionMenuChromeOS() = default;

void TouchSelectionMenuChromeOS::ActionButtonPressed() {
  ui::RecordTouchSelectionMenuSmartAction();
  auto* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager)
    return;
  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->intent_helper(), HandleIntent);
  if (!instance)
    return;

  instance->HandleIntent(std::move(action_->action_intent),
                         std::move(action_->activity));
}
