// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_profiles_view.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "ash/public/cpp/desk_profiles_delegate.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk.h"
#include "base/check_op.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"

namespace ash {

namespace {
// The size of desk profile avatar button.
constexpr gfx::Size kIconButtonSize(22, 22);
// The size for selected profile checker icon.
constexpr int kCheckButtonSize = 20;
// The size of desk profile icon on context menu item.
constexpr int kIconProfileSize = 24;

using ProfilesList = std::vector<LacrosProfileSummary>;
}  // namespace

class DeskProfilesMenuModelAdapter : public views::MenuModelAdapter {
 public:
  DeskProfilesMenuModelAdapter(ui::SimpleMenuModel* model,
                               base::RepeatingClosure menu_closed_callback,
                               DeskProfilesButton* button,
                               ui::MenuSourceType source_type,
                               ProfilesList* profiles)
      : views::MenuModelAdapter(model, std::move(menu_closed_callback)),
        profiles_(profiles),
        button_(button),
        source_type_(source_type) {}

  DeskProfilesMenuModelAdapter(const DeskProfilesMenuModelAdapter&) = delete;
  DeskProfilesMenuModelAdapter& operator=(const DeskProfilesMenuModelAdapter&) =
      delete;
  ~DeskProfilesMenuModelAdapter() override {}

  // Shows the menu anchored at `menu_anchor_position`. `run_types` is used for
  // the MenuRunner::RunTypes associated with the menu.`menu_anchor_rect`
  // indicates the bounds.
  void Run(const gfx::Rect& menu_anchor_rect,
           views::MenuAnchorPosition menu_anchor_position,
           int run_types) {
    menu_runner_ = std::make_unique<views::MenuRunner>(CreateMenu(), run_types);
    menu_runner_->RunMenuAt(/*parent=*/nullptr,
                            /* button_controller=*/nullptr, menu_anchor_rect,
                            menu_anchor_position, source_type_);
  }

 private:
  // views::MenuModelAdapter:
  // Override AppendMenuItem to use customized MenuItemView.
  views::MenuItemView* AppendMenuItem(views::MenuItemView* menu,
                                      ui::MenuModel* model,
                                      size_t index) override {
    auto* delegate = Shell::Get()->GetDeskProfilesDelegate();
    CHECK(delegate);
    const int command_id = model->GetCommandIdAt(index);
    CHECK_LT(index, profiles_->size());
    const auto& summary = (*profiles_)[index];
    views::MenuItemView* item_view = menu->AppendMenuItem(command_id);
    gfx::ImageSkia icon = gfx::ImageSkiaOperations::CreateResizedImage(
        summary.icon, skia::ImageOperations::RESIZE_BEST,
        gfx::Size(kIconProfileSize, kIconProfileSize));
    item_view->SetIcon(ui::ImageModel::FromImageSkia(
        gfx::ImageSkiaOperations::CreateImageWithRoundRectClip(kIconProfileSize,
                                                               icon)));
    item_view->SetTitle(base::UTF8ToUTF16(summary.name));
    // Add a secondary title for email if available. Note that local profile may
    // not have an associated email.
    if (!summary.email.empty()) {
      item_view->SetSecondaryTitle(base::UTF8ToUTF16(summary.email));
    }
    // Add a checker icon to the desk profile item that's assigned to.
    CHECK(button_->desk());
    if (button_->desk()->lacros_profile_id() == summary.profile_id) {
      item_view->AddChildView(
          views::Builder<views::BoxLayoutView>()
              .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
              .SetCrossAxisAlignment(
                  views::BoxLayout::CrossAxisAlignment::kCenter)
              .AddChild(views::Builder<views::ImageView>().SetImage(
                  ui::ImageModel::FromVectorIcon(kHollowCheckCircleIcon,
                                                 cros_tokens::kCrosSysPrimary,
                                                 kCheckButtonSize)))
              .Build());
    }
    return item_view;
  }
  // The list of logged in profiles.
  const raw_ptr<ProfilesList, ExperimentalAsh> profiles_;

  // The menu runner that is responsible to run the menu.
  // The avatar button.
  raw_ptr<DeskProfilesButton, ExperimentalAsh> button_;

  // The event type which was used to show the menu.
  const ui::MenuSourceType source_type_;
  // Responsible for showing menu.
  std::unique_ptr<views::MenuRunner> menu_runner_;
};

class DeskProfilesButton::MenuController : public ui::SimpleMenuModel::Delegate,
                                           public views::ContextMenuController {
 public:
  explicit MenuController(DeskProfilesButton* button)
      : context_menu_model_(this), profile_button_(button) {}
  MenuController(const MenuController&) = delete;
  MenuController& operator=(const MenuController&) = delete;
  ~MenuController() override = default;

  views::MenuRunner* menu_runner() { return menu_runner_.get(); }

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override {
    // TODO(shidi) : Update the command id to include other operations.
    CHECK_LT(command_id, static_cast<int>(profiles_.size()));
    profile_button_->desk_->SetLacrosProfileId(
        profiles_[command_id].profile_id);
  }

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override {
    int run_types = views::MenuRunner::USE_ASH_SYS_UI_LAYOUT |
                    views::MenuRunner::CONTEXT_MENU |
                    views::MenuRunner::FIXED_ANCHOR;
    BuildMenuModel();
    context_menu_adapter_ = std::make_unique<DeskProfilesMenuModelAdapter>(
        &context_menu_model_,
        base::BindRepeating(&DeskProfilesButton::MenuController::OnMenuClosed,
                            weak_ptr_factory_.GetWeakPtr()),
        profile_button_, source_type, &profiles_);
    context_menu_adapter_->Run(gfx::Rect(point, gfx::Size()),
                               views::MenuAnchorPosition::kBubbleBottomRight,
                               run_types);
  }

 private:
  // Builds and saves a default menu model to `context_menu_model_`;
  void BuildMenuModel() {
    auto* delegate = Shell::Get()->GetDeskProfilesDelegate();
    CHECK(delegate);

    profiles_ = delegate->GetProfilesSnapshot();
    // TODO(shidi): the index needs to be updated to separate profiles and other
    // commands.  Add function to generate index instead of using for loop iter.
    for (size_t index = 0; index < profiles_.size(); ++index) {
      context_menu_model_.AddItem(index,
                                  base::UTF8ToUTF16(profiles_[index].name));
    }
  }

  // Called when the context menu is closed. Used as a callback for
  // `menu_model_adapter_`.
  void OnMenuClosed() {
    menu_runner_.reset();
    context_menu_model_.Clear();
    context_menu_adapter_.reset();
  }

  // The context menu model and its adapter for `DeskProfilesButton`.
  ui::SimpleMenuModel context_menu_model_;
  std::unique_ptr<DeskProfilesMenuModelAdapter> context_menu_adapter_;

  // The current logged in profiles that displayed on the context menu.
  ProfilesList profiles_;

  // The menu runner that is responsible to run the menu.
  std::unique_ptr<views::MenuRunner> menu_runner_;

  // Owned by views hierarchy.
  raw_ptr<DeskProfilesButton, ExperimentalAsh> profile_button_ = nullptr;

  base::WeakPtrFactory<DeskProfilesButton::MenuController> weak_ptr_factory_{
      this};
};

DeskProfilesButton::DeskProfilesButton(views::Button::PressedCallback callback,
                                       Desk* desk)
    : desk_(desk) {
  desk_->AddObserver(this);
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  icon_ = AddChildView(std::make_unique<views::ImageView>());
  icon_->SetSize(kIconButtonSize);
  icon_->SetImageSize(kIconButtonSize);
  UpdateIcon();
  icon_->SetPaintToLayer();
  icon_->layer()->SetFillsBoundsOpaquely(false);
  icon_->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(kIconButtonSize.width()));
  // TODO(shidi):Update the accessible name if get any.
  SetAccessibleName(u"", ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
}

DeskProfilesButton::~DeskProfilesButton() {
  if (desk_) {
    desk_->RemoveObserver(this);
  }
}

void DeskProfilesButton::UpdateIcon() {
  CHECK(desk_);
  auto* delegate = Shell::Get()->GetDeskProfilesDelegate();
  CHECK(delegate);
  // Initialize Desk's Lacros profile id with primary profile id.
  const uint64_t primary_profile_id = delegate->GetPrimaryProfileId();
  if (desk_->lacros_profile_id() == 0 && primary_profile_id != 0) {
    desk_->SetLacrosProfileId(primary_profile_id);
  }
  if (auto* summary = delegate->GetProfilesSnapshotByProfileId(
          desk_->lacros_profile_id())) {
    icon_image_ = summary->icon;
    icon_->SetImage(icon_image_);
  }
}

bool DeskProfilesButton::IsMenuShowing() const {
  CHECK(context_menu_);
  auto* menu_runner = context_menu_->menu_runner();
  return menu_runner && menu_runner->IsRunning();
}

void DeskProfilesButton::OnDeskDestroyed(const Desk* desk) {
  // Note that DeskProfilesButton's parent `DeskMiniView` might outlive the
  // `desk_`, so `desk_` need to be manually reset.
  desk_ = nullptr;
}

void DeskProfilesButton::OnMouseReleased(const ui::MouseEvent& event) {
  if (event.IsLeftMouseButton()) {
    CreateMenu(event);
  }
}

void DeskProfilesButton::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
    CreateMenu(*event);
  }
}

void DeskProfilesButton::CreateMenu(const ui::LocatedEvent& event) {
  gfx::Point location_in_screen(event.location());
  View::ConvertPointToScreen(this, &location_in_screen);
  if (!context_menu_) {
    context_menu_ = std::make_unique<MenuController>(this);
    set_context_menu_controller(context_menu_.get());
  }
  context_menu_->ShowContextMenuForViewImpl(this, location_in_screen,
                                            ui::MENU_SOURCE_MOUSE);
}

}  // namespace ash
