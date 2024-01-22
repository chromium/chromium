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
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "base/check_op.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/highlight_path_generator.h"
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
// We are using the actual value of`IDC_MANAGE_CHROME_PROFILES` here because
// `IDC_MANAGE_CHROME_PROFILES` is defined in chrome header, so we can't include
// it here. And we also want to use a unique number to avoid duplication with
// auto generated `command_id` from profile index above (ranges from 0 to
// profile count limit).
constexpr int IDC_ASH_DESKS_OPEN_PROFILE_MANAGER = 35358;

using ProfilesList = std::vector<LacrosProfileSummary>;
}  // namespace

// -----------------------------------------------------------------------------
// DeskProfilesMenuModelAdapter:

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
  ~DeskProfilesMenuModelAdapter() override {
    menu_runner_.reset();
    root_menu_item_view_ = nullptr;
  }

  views::MenuRunner* menu_runner() { return menu_runner_.get(); }
  views::MenuItemView* root_menu_item_view() const {
    return root_menu_item_view_;
  }

  // Shows the menu anchored at `menu_anchor_position`. `run_types` is used for
  // the MenuRunner::RunTypes associated with the menu.`menu_anchor_rect`
  // indicates the bounds.
  void Run(const gfx::Rect& menu_anchor_rect,
           views::MenuAnchorPosition menu_anchor_position,
           int run_types) {
    std::unique_ptr<views::MenuItemView> menu = CreateMenu();
    root_menu_item_view_ = menu.get();
    menu_runner_ =
        std::make_unique<views::MenuRunner>(std::move(menu), run_types);
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
    if (model->GetTypeAt(index) == ui::MenuModel::TYPE_SEPARATOR) {
      menu->AppendSeparator();
      return nullptr;
    }
    const int command_id = model->GetCommandIdAt(index);
    views::MenuItemView* item_view = menu->AppendMenuItem(command_id);
    if (command_id == IDC_ASH_DESKS_OPEN_PROFILE_MANAGER) {
      item_view->SetIcon(ui::ImageModel::FromVectorIcon(
          kSettingsIcon, cros_tokens::kCrosSysOnSurface, kCheckButtonSize));
      item_view->SetTitle(
          l10n_util::GetStringUTF16(IDS_ASH_DESKS_OPEN_PROFILE_MANAGER));
    } else {
      // Update each profile item view with customized style.
      CHECK_LT(command_id, static_cast<int>(profiles_->size()));
      const auto& summary = (*profiles_)[command_id];
      gfx::ImageSkia icon = gfx::ImageSkiaOperations::CreateResizedImage(
          summary.icon, skia::ImageOperations::RESIZE_BEST,
          gfx::Size(kIconProfileSize, kIconProfileSize));
      item_view->SetIcon(ui::ImageModel::FromImageSkia(
          gfx::ImageSkiaOperations::CreateImageWithRoundRectClip(
              kIconProfileSize, icon)));
      item_view->SetTitle(base::UTF8ToUTF16(summary.name));
      item_view->SetHighlightWhenSelectedWithChildViews(true);
      // Add a secondary title for email if available. Note that local profile
      // may not have an associated email.
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
    }
    return item_view;
  }

  // The list of logged in profiles.
  const raw_ptr<ProfilesList> profiles_;
  // The avatar button.
  raw_ptr<DeskProfilesButton> button_;
  // The event type which was used to show the menu.
  const ui::MenuSourceType source_type_;
  // The root menu item view. Cached for testing.
  raw_ptr<views::MenuItemView> root_menu_item_view_ = nullptr;
  // Responsible for showing menu.
  std::unique_ptr<views::MenuRunner> menu_runner_;
};

// -----------------------------------------------------------------------------
// DeskProfilesButton::MenuController:

class DeskProfilesButton::MenuController : public ui::SimpleMenuModel::Delegate,
                                           public views::ContextMenuController {
 public:
  explicit MenuController(DeskProfilesButton* button)
      : context_menu_model_(this), profile_button_(button) {}
  MenuController(const MenuController&) = delete;
  MenuController& operator=(const MenuController&) = delete;
  ~MenuController() override = default;

  views::MenuRunner* menu_runner() {
    return context_menu_adapter_->menu_runner();
  }

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override {
    if (command_id == IDC_ASH_DESKS_OPEN_PROFILE_MANAGER) {
      base::UmaHistogramBoolean(kDeskProfilesOpenProfileManagerHistogramName,
                                true);
      Shell::Get()->shell_delegate()->OpenProfileManager();
      return;
    }
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
  friend class TestApi;
  // Builds and saves a default menu model to `context_menu_model_`;
  void BuildMenuModel() {
    auto* delegate = Shell::Get()->GetDeskProfilesDelegate();
    if (!delegate) {
      // For Ash unit test there is no delegate available.
      return;
    }

    profiles_ = delegate->GetProfilesSnapshot();
    for (size_t index = 0; index < profiles_.size(); ++index) {
      context_menu_model_.AddItem(index,
                                  base::UTF8ToUTF16(profiles_[index].name));
    }
    context_menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
    context_menu_model_.AddItem(
        IDC_ASH_DESKS_OPEN_PROFILE_MANAGER,
        l10n_util::GetStringUTF16(IDS_ASH_DESKS_OPEN_PROFILE_MANAGER));
  }

  // Called when the context menu is closed. Used as a callback for
  // `menu_model_adapter_`.
  void OnMenuClosed() {
    context_menu_model_.Clear();
    context_menu_adapter_.reset();
  }

  // The context menu model and its adapter for `DeskProfilesButton`.
  ui::SimpleMenuModel context_menu_model_;
  std::unique_ptr<DeskProfilesMenuModelAdapter> context_menu_adapter_;

  // The current logged in profiles that displayed on the context menu.
  ProfilesList profiles_;

  // Owned by views hierarchy.
  raw_ptr<DeskProfilesButton> profile_button_ = nullptr;

  base::WeakPtrFactory<DeskProfilesButton::MenuController> weak_ptr_factory_{
      this};
};

// -----------------------------------------------------------------------------
// DeskProfilesButton::TestApi:

views::MenuItemView* DeskProfilesButton::TestApi::GetMenuItemByID(int id) {
  return button_->context_menu_->context_menu_adapter_->root_menu_item_view()
      ->GetMenuItemByID(id);
}

// -----------------------------------------------------------------------------
// DeskProfilesButton:

DeskProfilesButton::DeskProfilesButton(views::Button::PressedCallback callback,
                                       Desk* desk)
    : desk_(desk) {
  desk_->AddObserver(this);
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  SetPreferredSize(kIconButtonSize);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  icon_ = AddChildView(std::make_unique<views::ImageView>());
  icon_->SetSize(kIconButtonSize);
  icon_->SetImageSize(kIconButtonSize);
  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetOutsetFocusRingDisabled(true);
  focus_ring->SetColorId(cros_tokens::kCrosSysFocusRing);
  focus_ring->SetPathGenerator(
      std::make_unique<views::CircleHighlightPathGenerator>(
          -gfx::Insets(focus_ring->GetHaloThickness() / 2)));
  views::InstallCircleHighlightPathGenerator(this);

  UpdateIcon();
  icon_->SetPaintToLayer();
  icon_->layer()->SetFillsBoundsOpaquely(false);
  icon_->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(kIconButtonSize.width()));
  // TODO(shidi):Update the accessible name if get any.
  SetAccessibleName(u"", ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
}

DeskProfilesButton::~DeskProfilesButton() {
  set_context_menu_controller(nullptr);
  if (desk_) {
    desk_->RemoveObserver(this);
  }
}

void DeskProfilesButton::UpdateIcon() {
  CHECK(desk_);
  auto* delegate = Shell::Get()->GetDeskProfilesDelegate();
  if (!delegate) {
    // For Ash unit test there is no delegate available.
    return;
  }
  // Initialize Desk's Lacros profile id with primary profile id.
  const uint64_t primary_profile_id = delegate->GetPrimaryProfileId();
  if (desk_->lacros_profile_id() == 0 && primary_profile_id != 0) {
    desk_->SetLacrosProfileId(primary_profile_id);
  }
  if (auto* summary = delegate->GetProfilesSnapshotByProfileId(
          desk_->lacros_profile_id())) {
    icon_image_ = summary->icon;
    icon_->SetImage(icon_image_);
    icon_->SetTooltipText(base::UTF8ToUTF16(summary->name));
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

bool DeskProfilesButton::OnMousePressed(const ui::MouseEvent& event) {
  base::UmaHistogramBoolean(kDeskProfilesPressesHistogramName, true);
  if (event.IsLeftMouseButton()) {
    CreateMenu(event);
  }
  return ImageButton::OnMousePressed(event);
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

BEGIN_METADATA(DeskProfilesButton)
END_METADATA

}  // namespace ash
