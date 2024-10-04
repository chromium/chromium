// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/power_button.h"

#include <utility>

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shutdown_controller_impl.h"
#include "ash/shutdown_reason.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/style/style_util.h"
#include "ash/style/typography.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/unified/quick_settings_metrics_util.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/user_chooser_detailed_view_controller.h"
#include "ash/wm/lock_state_controller.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "components/user_manager/user_type.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

namespace ash {

namespace {
// Rounded corner constants.
static constexpr int kRoundedCornerRadius = 16;
static constexpr int kNonRoundedCornerRadius = 4;

// Size of the icon in the power button.
constexpr gfx::Size kIconSize{20, 20};

constexpr gfx::RoundedCornersF kBottomRightNonRoundedCorners(
    kRoundedCornerRadius,
    kRoundedCornerRadius,
    kNonRoundedCornerRadius,
    kRoundedCornerRadius);

constexpr gfx::RoundedCornersF kBottomLeftNonRoundedCorners(
    kRoundedCornerRadius,
    kRoundedCornerRadius,
    kRoundedCornerRadius,
    kNonRoundedCornerRadius);

constexpr gfx::RoundedCornersF kTopLeftNonRoundedCorners(
    kNonRoundedCornerRadius,
    kRoundedCornerRadius,
    kRoundedCornerRadius,
    kRoundedCornerRadius);

constexpr gfx::RoundedCornersF kTopRightNonRoundedCorners(
    kRoundedCornerRadius,
    kNonRoundedCornerRadius,
    kRoundedCornerRadius,
    kRoundedCornerRadius);

constexpr gfx::RoundedCornersF kAllRoundedCorners(kRoundedCornerRadius,
                                                  kRoundedCornerRadius,
                                                  kRoundedCornerRadius,
                                                  kRoundedCornerRadius);

// The highlight path generator for the `PowerButton`.
class HighlightPathGenerator : public views::HighlightPathGenerator {
 public:
  explicit HighlightPathGenerator(PowerButton* power_button)
      : power_button_(power_button) {}
  HighlightPathGenerator(const HighlightPathGenerator&) = delete;
  HighlightPathGenerator& operator=(const HighlightPathGenerator&) = delete;

 private:
  // HighlightPathGenerator:
  std::optional<gfx::RRectF> GetRoundRect(const gfx::RectF& rect) override {
    gfx::RectF bounds(power_button_->GetLocalBounds());
    gfx::RoundedCornersF rounded = kAllRoundedCorners;
    if (power_button_->IsMenuShowing()) {
      // Don't need to check RTL since the `HighlightPathGenerator` will auto
      // adjust the shape for the RTL case.
      rounded = kTopLeftNonRoundedCorners;
    }

    return gfx::RRectF(bounds, rounded);
  }

  // Owned by views hierarchy.
  const raw_ptr<PowerButton> power_button_;
};

// Returns whether the user's email address should be shown in the power menu.
bool ShouldShowEmailMenuItem() {
  const UserSession* user_session =
      Shell::Get()->session_controller()->GetPrimaryUserSession();
  // Don't show if no user is signed in.
  if (!user_session) {
    return false;
  }
  switch (user_session->user_info.type) {
    case user_manager::UserType::kRegular:
    case user_manager::UserType::kChild:
      return true;
    case user_manager::UserType::kGuest:
    case user_manager::UserType::kPublicAccount:
    case user_manager::UserType::kKioskApp:
    case user_manager::UserType::kWebKioskApp:
    case user_manager::UserType::kKioskIWA:
      return false;
  }
}

// Returns the text for the email address item in the power menu.
std::u16string GetEmailMenuItemText() {
  // The 0th user session is the current one.
  const UserSession* user_session =
      Shell::Get()->session_controller()->GetUserSession(/*index=*/0);
  CHECK(user_session);
  return base::UTF8ToUTF16(user_session->user_info.display_email);
}

// The menu delegate for power button menu, which overrides the fontlist for
// menu labels.
class PowerButtonMenuDelegate : public ui::SimpleMenuModel {
 public:
  explicit PowerButtonMenuDelegate(Delegate* delegate)
      : ui::SimpleMenuModel(delegate) {
    font_list_ = ash::TypographyProvider::Get()->ResolveTypographyToken(
        ash::TypographyToken::kCrosButton2);
  }
  PowerButtonMenuDelegate(const PowerButtonMenuDelegate&) = delete;
  PowerButtonMenuDelegate& operator=(const PowerButtonMenuDelegate&) = delete;
  ~PowerButtonMenuDelegate() override = default;

  // ui::MenuModel
  const gfx::FontList* GetLabelFontListAt(size_t index) const override {
    return &font_list_;
  }

  gfx::FontList font_list_;
};
}  // namespace

class PowerButton::MenuController : public ui::SimpleMenuModel::Delegate,
                                    public views::ContextMenuController {
 public:
  explicit MenuController(PowerButton* button) : power_button_(button) {}
  MenuController(const MenuController&) = delete;
  MenuController& operator=(const MenuController&) = delete;
  ~MenuController() override = default;

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdEnabled(int command_id) const override {
    if (command_id == VIEW_ID_QS_POWER_EMAIL_MENU_BUTTON) {
      // Enable the email item if OS multi-profile is available.
      return UserChooserDetailedViewController::IsUserChooserEnabled();
    }
    return true;
  }

  void ExecuteCommand(int command_id, int event_flags) override {
    switch (command_id) {
      case VIEW_ID_QS_POWER_EMAIL_MENU_BUTTON:
        quick_settings_metrics_util::RecordQsButtonActivated(
            QsButtonCatalogName::kPowerEmailMenuButton);
        power_button_->tray_controller_->ShowUserChooserView();
        break;
      case VIEW_ID_QS_POWER_OFF_MENU_BUTTON:
        quick_settings_metrics_util::RecordQsButtonActivated(
            QsButtonCatalogName::kPowerOffMenuButton);
        Shell::Get()->lock_state_controller()->RequestShutdown(
            ShutdownReason::TRAY_SHUT_DOWN_BUTTON);
        break;
      case VIEW_ID_QS_POWER_SIGNOUT_MENU_BUTTON:
        quick_settings_metrics_util::RecordQsButtonActivated(
            QsButtonCatalogName::kPowerSignoutMenuButton);
        Shell::Get()->lock_state_controller()->RequestSignOut();
        break;
      case VIEW_ID_QS_POWER_RESTART_MENU_BUTTON:
        quick_settings_metrics_util::RecordQsButtonActivated(
            QsButtonCatalogName::kPowerRestartMenuButton);
        Shell::Get()->lock_state_controller()->RequestRestart(
            power_manager::REQUEST_RESTART_FOR_USER, "Reboot by user");
        break;
      case VIEW_ID_QS_POWER_LOCK_MENU_BUTTON:
        quick_settings_metrics_util::RecordQsButtonActivated(
            QsButtonCatalogName::kPowerLockMenuButton);
        Shell::Get()->session_controller()->LockScreen();
        break;
      default:
        NOTREACHED();
    }
  }

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override {
    // Build the menu model and save it to `context_menu_model_`.
    BuildMenuModel();
    menu_model_adapter_ = std::make_unique<views::MenuModelAdapter>(
        context_menu_model_.get(),
        base::BindRepeating(&MenuController::OnMenuClosed,
                            base::Unretained(this)));
    std::unique_ptr<views::MenuItemView> menu =
        menu_model_adapter_->CreateMenu();
    root_menu_item_view_ = menu.get();
    int run_types = views::MenuRunner::USE_ASH_SYS_UI_LAYOUT |
                    views::MenuRunner::CONTEXT_MENU |
                    views::MenuRunner::FIXED_ANCHOR;
    menu_runner_ =
        std::make_unique<views::MenuRunner>(std::move(menu), run_types);

    menu_runner_->RunMenuAt(source->GetWidget(), /*button_controller=*/nullptr,
                            source->GetBoundsInScreen(),
                            views::MenuAnchorPosition::kBubbleTopRight,
                            source_type, /*native_view_for_gestures=*/nullptr,
                            /*corners=*/
                            base::i18n::IsRTL() ? kBottomRightNonRoundedCorners
                                                : kBottomLeftNonRoundedCorners);
  }

  // Builds and saves a SimpleMenuModel to `context_menu_model_`;
  void BuildMenuModel() {
    // `context_menu_model_` and the other related pointers will be live for one
    // menu view's life cycle. This model will be built based on the use case
    // right before the menu view is shown. For example in the non-logged in
    // page, we only build power off and restart button.
    context_menu_model_ =
        std::make_unique<PowerButtonMenuDelegate>(/*delegate=*/this);

    SessionControllerImpl* session_controller =
        Shell::Get()->session_controller();
    bool const is_on_login_screen =
        session_controller->login_status() == LoginStatus::NOT_LOGGED_IN;
    bool const can_show_settings = TrayPopupUtils::CanOpenWebUISettings();
    bool const can_lock_screen = session_controller->CanLockScreen();
    bool const show_power_off_button =
        !Shell::Get()->shutdown_controller()->reboot_on_shutdown();

    // Add the user's email address (which is also the entry point for OS-level
    // multi-profile).
    if (ShouldShowEmailMenuItem()) {
      context_menu_model_->AddItemWithIcon(
          VIEW_ID_QS_POWER_EMAIL_MENU_BUTTON, GetEmailMenuItemText(),
          ui::ImageModel::FromVectorIcon(kSystemMenuNewUserIcon,
                                         cros_tokens::kCrosSysOnSurface,
                                         kTrayTopShortcutButtonIconSize));
      context_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    }

    if (show_power_off_button) {
      context_menu_model_->AddItemWithIcon(
          VIEW_ID_QS_POWER_OFF_MENU_BUTTON,
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SHUTDOWN),
          ui::ImageModel::FromVectorIcon(kSystemPowerButtonMenuPowerOffIcon,
                                         cros_tokens::kCrosSysOnSurface,
                                         kTrayTopShortcutButtonIconSize));
    }

    context_menu_model_->AddItemWithIcon(
        VIEW_ID_QS_POWER_RESTART_MENU_BUTTON,
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_REBOOT),
        ui::ImageModel::FromVectorIcon(kSystemPowerButtonMenuRestartIcon,
                                       cros_tokens::kCrosSysOnSurface,
                                       kTrayTopShortcutButtonIconSize));
    if (!is_on_login_screen) {
      context_menu_model_->AddItemWithIcon(
          VIEW_ID_QS_POWER_SIGNOUT_MENU_BUTTON,
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SIGN_OUT),
          ui::ImageModel::FromVectorIcon(kSystemPowerButtonMenuSignOutIcon,
                                         cros_tokens::kCrosSysOnSurface,
                                         kTrayTopShortcutButtonIconSize));
    }
    if (can_show_settings && can_lock_screen) {
      context_menu_model_->AddItemWithIcon(
          VIEW_ID_QS_POWER_LOCK_MENU_BUTTON,
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_LOCK),
          ui::ImageModel::FromVectorIcon(kSystemPowerButtonMenuLockScreenIcon,
                                         cros_tokens::kCrosSysOnSurface,
                                         kTrayTopShortcutButtonIconSize));
    }
  }

  // Called when the context menu is closed. Used as a callback for
  // `menu_model_adapter_`.
  void OnMenuClosed() {
    root_menu_item_view_ = nullptr;
    menu_runner_.reset();
    context_menu_model_.reset();
    menu_model_adapter_.reset();
    power_button_->UpdateView();
  }

  // The context menu model and its adapter for `PowerButton`.
  std::unique_ptr<ui::SimpleMenuModel> context_menu_model_;
  std::unique_ptr<views::MenuModelAdapter> menu_model_adapter_;

  // The menu runner that is responsible to run the menu.
  std::unique_ptr<views::MenuRunner> menu_runner_;

  // The root menu item view of `context_menu_model_`. Cached for testing.
  raw_ptr<views::MenuItemView> root_menu_item_view_ = nullptr;

  // Owned by views hierarchy.
  raw_ptr<PowerButton> power_button_ = nullptr;
};

PowerButtonContainer::PowerButtonContainer(PressedCallback callback)
    : Button(std::move(callback)) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout->SetOrientation(views::BoxLayout::Orientation::kHorizontal);

  power_icon_ = AddChildView(std::make_unique<views::ImageView>());
  power_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      kUnifiedMenuPowerIcon, cros_tokens::kCrosSysOnSurface));
  power_icon_->SetImageSize(kIconSize);
  arrow_icon_ = AddChildView(std::make_unique<views::ImageView>());
  arrow_icon_->SetID(VIEW_ID_QS_POWER_BUTTON_CHEVRON_ICON);
  arrow_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      kChevronDownSmallIcon, cros_tokens::kCrosSysOnSurface));
  arrow_icon_->SetImageSize(kIconSize);

  SetBorder(views::CreateEmptyBorder(gfx::Insets(6)));

  // Paints this view to a layer so it will be on top of the
  // `background_view_`
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_POWER_MENU));
  SetTooltipText(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_POWER_MENU));
}

PowerButtonContainer::~PowerButtonContainer() = default;

void PowerButtonContainer::UpdateIconColor(bool is_active) {
  auto icon_color_id = is_active ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                                 : cros_tokens::kCrosSysOnSurface;
  power_icon_->SetImage(
      ui::ImageModel::FromVectorIcon(kUnifiedMenuPowerIcon, icon_color_id));
  arrow_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      is_active ? kChevronUpSmallIcon : kChevronDownSmallIcon, icon_color_id));
}

BEGIN_METADATA(PowerButtonContainer)
END_METADATA

PowerButton::PowerButton(UnifiedSystemTrayController* tray_controller)
    : background_view_(AddChildView(std::make_unique<View>())),
      button_content_(AddChildView(std::make_unique<PowerButtonContainer>(
          base::BindRepeating(&PowerButton::OnButtonActivated,
                              base::Unretained(this))))),
      context_menu_(std::make_unique<MenuController>(/*button=*/this)),
      tray_controller_(tray_controller) {
  CHECK(tray_controller_);

  SetID(VIEW_ID_QS_POWER_BUTTON);
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Inits the `background_view_`'s layer. This view is `SetPaintToLayer` so it
  // can be set the customized rounded corner.
  background_view_->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  auto* background_layer = background_view_->layer();
  background_layer->SetRoundedCornerRadius(kAllRoundedCorners);
  background_layer->SetFillsBoundsOpaquely(false);
  background_layer->SetIsFastRoundedCorner(true);

  set_context_menu_controller(context_menu_.get());

  // Installs the customized focus ring path generator for the button.
  views::HighlightPathGenerator::Install(
      button_content_,
      std::make_unique<HighlightPathGenerator>(/*power_button=*/this));
  views::FocusRing::Get(button_content_)
      ->SetColorId(cros_tokens::kCrosSysPrimary);

  // Ripple.
  StyleUtil::SetUpInkDropForButton(button_content_, gfx::Insets(),
                                   /*highlight_on_hover=*/false,
                                   /*highlight_on_focus=*/false);
}

PowerButton::~PowerButton() {
  set_context_menu_controller(nullptr);
}

bool PowerButton::IsMenuShowing() {
  auto* menu_runner = context_menu_->menu_runner_.get();
  return menu_runner && menu_runner->IsRunning();
}

views::MenuItemView* PowerButton::GetMenuViewForTesting() {
  return context_menu_->root_menu_item_view_;
}

void PowerButton::OnThemeChanged() {
  views::View::OnThemeChanged();

  SkColor inactive_color =
      GetColorProvider()->GetColor(cros_tokens::kCrosSysSystemOnBase);
  SkColor active_color =
      GetColorProvider()->GetColor(cros_tokens::kCrosSysSystemPrimaryContainer);
  background_view_->layer()->SetColor(IsMenuShowing() ? active_color
                                                      : inactive_color);
}

void PowerButton::UpdateView() {
  UpdateRoundedCorners();
  OnThemeChanged();
  views::FocusRing* focus_ring = views::FocusRing::Get(button_content_);
  if (button_content_->HasFocus() && focus_ring) {
    // Updating the focus ring path, make sure the focus ring gets updated to
    // match this new state.
    focus_ring->InvalidateLayout();
    focus_ring->SchedulePaint();
  }
  button_content_->UpdateIconColor(/*is_active*/ IsMenuShowing());
}

void PowerButton::UpdateRoundedCorners() {
  gfx::RoundedCornersF corners = kAllRoundedCorners;
  if (IsMenuShowing()) {
    corners = base::i18n::IsRTL() ? kTopRightNonRoundedCorners
                                  : kTopLeftNonRoundedCorners;
  }

  background_view_->layer()->SetRoundedCornerRadius(corners);
}

void PowerButton::OnButtonActivated(const ui::Event& event) {
  quick_settings_metrics_util::RecordQsButtonActivated(
      QsButtonCatalogName::kPowerButton);
  ui::MenuSourceType type;

  if (event.IsMouseEvent()) {
    type = ui::MENU_SOURCE_MOUSE;
  } else if (event.IsTouchEvent()) {
    type = ui::MENU_SOURCE_TOUCH;
  } else if (event.IsKeyEvent()) {
    type = ui::MENU_SOURCE_KEYBOARD;
  } else {
    type = ui::MENU_SOURCE_STYLUS;
  }

  context_menu_->ShowContextMenuForView(
      /*source=*/this, GetBoundsInScreen().CenterPoint(), type);

  UpdateView();
}

BEGIN_METADATA(PowerButton)
END_METADATA

}  // namespace ash
