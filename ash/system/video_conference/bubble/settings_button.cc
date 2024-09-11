// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/bubble/settings_button.h"

#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/switch.h"
#include "ash/system/camera/camera_effects_controller.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/video_conference/video_conference_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/style/typography_provider.h"

namespace ash::video_conference {

namespace {

// Rounded corner constants.
static constexpr int kRoundedCornerRadius = 16;
static constexpr int kNonRoundedCornerRadius = 4;
static constexpr int kIconSize = 20;
static constexpr int kIconSpacing = 16;
static constexpr int kMenuItemTopBottomPadding = 4;

enum CommandId {
  kAudioSettings = 1,
  kPrivacySettings = 2,
  kPortraitRelighting = 3,
  kFaceRetouch = 4,
};

constexpr gfx::Size kIconSizeGfx{kIconSize, kIconSize};

constexpr gfx::RoundedCornersF kTopRightNonRoundedCorners(
    kRoundedCornerRadius,
    kNonRoundedCornerRadius,
    kRoundedCornerRadius,
    kRoundedCornerRadius);

// A MenuItemView with a toggle. This is used for the Studio Look preference
// menu items.
class SwitchMenuItemView : public views::MenuItemView {
  METADATA_HEADER(SwitchMenuItemView, views::MenuItemView)

 public:
  SwitchMenuItemView(MenuItemView* parent,
                     int command_id,
                     const std::u16string& title)
      : views::MenuItemView(parent,
                            command_id,
                            views::MenuItemView::Type::kNormal) {
    // Creates a non-clickable non-focusable switch. The events and focus
    // behavior are handled by its parent.
    switch_ = AddChildView(std::make_unique<Switch>());
    switch_->SetIsOn(GetDelegate()->IsItemChecked(GetCommand()));
    switch_->SetCanProcessEventsWithinSubtree(false);
    switch_->SetFocusBehavior(views::View::FocusBehavior::NEVER);
    auto& view_accessibility = switch_->GetViewAccessibility();
    view_accessibility.SetIsLeaf(true);
    view_accessibility.SetIsIgnored(true);

    SetTitle(title);
    UpdateAccessibleName();

    // Adding a custom child view breaks highlighting. This is a workaround to
    // make highlighting work properly.
    SetHighlightWhenSelectedWithChildViews(true);
  }

  void UpdateAccessibleCheckedState() override {
    if (switch_) {
      switch_->AnimateIsOn(!switch_->GetIsOn());
      UpdateAccessibleName();
    }
  }

 private:
  void UpdateAccessibleName() {
    GetViewAccessibility().SetName(l10n_util::GetStringFUTF16(
        IDS_ASH_VIDEO_CONFERENCE_PREFERENCE_STATE_ACCESSIBLE_NAME,
        l10n_util::GetStringUTF16(
            IDS_ASH_VIDEO_CONFERENCE_SETTINGS_STUDIO_LOOK_PREFERENCE),
        title(),
        l10n_util::GetStringUTF16(
            switch_->GetIsOn() ? VIDEO_CONFERENCE_TOGGLE_BUTTON_STATE_ON
                               : VIDEO_CONFERENCE_TOGGLE_BUTTON_STATE_OFF)));
  }

  raw_ptr<ash::Switch> switch_ = nullptr;
};

BEGIN_METADATA(SwitchMenuItemView)
END_METADATA

class SettingsMenuModelAdapter : public views::MenuModelAdapter {
 public:
  explicit SettingsMenuModelAdapter(
      ui::MenuModel* menu_model,
      base::RepeatingClosure on_menu_closed_callback = base::NullCallback())
      : views::MenuModelAdapter(menu_model, on_menu_closed_callback) {}

  SettingsMenuModelAdapter(const SettingsMenuModelAdapter&) = delete;
  SettingsMenuModelAdapter& operator=(const SettingsMenuModelAdapter&) = delete;

 protected:
  views::MenuItemView* AppendMenuItem(views::MenuItemView* menu,
                                      ui::MenuModel* model,
                                      size_t index) override {
    int command_id = model->GetCommandIdAt(index);
    if (model->GetTypeAt(index) == ui::MenuModel::ItemType::TYPE_TITLE) {
      // Appends MenuItemView for Studio Look preference title.
      views::MenuItemView* container = menu->AppendMenuItem(command_id);
      container->AddChildView(
          views::Builder<views::BoxLayoutView>()
              .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
              .SetInsideBorderInsets(gfx::Insets()
                                         .set_top(kMenuItemTopBottomPadding)
                                         .set_bottom(kMenuItemTopBottomPadding)
                                         .set_left(kIconSpacing))
              .SetBetweenChildSpacing(kIconSpacing)
              .AddChild(views::Builder<views::ImageView>()
                            .SetImage(ui::ImageModel::FromVectorIcon(
                                kVideoConferenceStudioLookIcon,
                                cros_tokens::kCrosSysOnSurface))
                            .SetImageSize(kIconSizeGfx))
              .AddChild(
                  views::Builder<views::Label>()
                      .SetText(l10n_util::GetStringUTF16(
                          IDS_ASH_VIDEO_CONFERENCE_SETTINGS_STUDIO_LOOK_PREFERENCE))
                      .SetFontList(views::TypographyProvider::Get().GetFont(
                          views::style::CONTEXT_TOUCH_MENU,
                          views::style::STYLE_PRIMARY)))
              .Build());
      container->GetViewAccessibility().SetIsIgnored(true);
      return container;
    }

    if (model->GetTypeAt(index) == ui::MenuModel::ItemType::TYPE_CHECK) {
      // Appends SwitchMenuItemView, which is a MenuItemView with a toggle.
      views::MenuItemView* container =
          menu->GetSubmenu()->AddChildView(std::make_unique<SwitchMenuItemView>(
              menu, command_id, model->GetLabelAt(index)));
      if (command_id == CommandId::kPortraitRelighting) {
        portrait_relighting_menu_item_view_ = container;
      } else if (command_id == CommandId::kFaceRetouch) {
        face_retouch_menu_item_view_ = container;
      }
      return container;
    }

    return AppendMenuItemFromModel(model, index, menu, command_id);
  }

  void ExecuteCommand(int command_id) override {
    ExecuteCommand(command_id, /*event_flags=*/0);
  }

  void ExecuteCommand(int command_id, int event_flags) override {
    views::MenuModelAdapter::ExecuteCommand(command_id, event_flags);
    switch (command_id) {
      case CommandId::kPortraitRelighting:
        if (portrait_relighting_menu_item_view_) {
          portrait_relighting_menu_item_view_->UpdateAccessibleCheckedState();
        }
        break;
      case CommandId::kFaceRetouch:
        if (face_retouch_menu_item_view_) {
          face_retouch_menu_item_view_->UpdateAccessibleCheckedState();
        }
        break;
    }
  }

  bool ShouldExecuteCommandWithoutClosingMenu(int command_id,
                                              const ui::Event& event) override {
    // The menu should not be closed when executing SwitchMenuItemView's
    // command.
    return command_id == CommandId::kPortraitRelighting ||
           command_id == CommandId::kFaceRetouch;
  }

  void OnMenuClosed(views::MenuItemView* menu) override {
    // Prevents dangling pointers.
    portrait_relighting_menu_item_view_ = nullptr;
    face_retouch_menu_item_view_ = nullptr;
    views::MenuModelAdapter::OnMenuClosed(menu);
  }

 private:
  raw_ptr<views::MenuItemView> portrait_relighting_menu_item_view_ = nullptr;
  raw_ptr<views::MenuItemView> face_retouch_menu_item_view_ = nullptr;
};

}  // namespace

class SettingsButton::MenuController : public ui::SimpleMenuModel::Delegate,
                                       public views::ContextMenuController {
 public:
  explicit MenuController(base::OnceClosure close_bubble_callback)
      : close_bubble_callback_(std::move(close_bubble_callback)) {}

  MenuController(const MenuController&) = delete;
  MenuController& operator=(const MenuController&) = delete;
  ~MenuController() override = default;

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override {
    std::optional<int> state;
    if (command_id == CommandId::kPortraitRelighting) {
      state =
          effects_controller_->GetEffectState(VcEffectId::kPortraitRelighting);
    } else if (command_id == CommandId::kFaceRetouch) {
      state = effects_controller_->GetEffectState(VcEffectId::kFaceRetouch);
    } else {
      return false;
    }
    return *state != 0;
  }

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override {
    auto* client = Shell::Get()->system_tray_model()->client();
    switch (command_id) {
      case CommandId::kAudioSettings:
        std::move(close_bubble_callback_).Run();
        client->ShowAudioSettings();
        break;
      case CommandId::kPrivacySettings:
        std::move(close_bubble_callback_).Run();
        client->ShowPrivacyAndSecuritySettings();
        break;
      case CommandId::kPortraitRelighting:
        base::UmaHistogramBoolean(
            video_conference_utils::GetEffectHistogramNameForClick(
                VcEffectId::kPortraitRelighting),
            !IsCommandIdChecked(command_id));
        effects_controller_->OnEffectControlActivated(
            VcEffectId::kPortraitRelighting, /*state=*/std::nullopt);
        break;
      case CommandId::kFaceRetouch:
        base::UmaHistogramBoolean(
            video_conference_utils::GetEffectHistogramNameForClick(
                VcEffectId::kFaceRetouch),
            !IsCommandIdChecked(command_id));
        effects_controller_->OnEffectControlActivated(VcEffectId::kFaceRetouch,
                                                      /*state=*/std::nullopt);
        break;
    }
  }

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override {
    BuildMenuModel();
    menu_model_adapter_ = std::make_unique<SettingsMenuModelAdapter>(
        menu_model_.get(), base::BindRepeating(&MenuController::OnMenuClosed,
                                               base::Unretained(this)));
    std::unique_ptr<views::MenuItemView> menu =
        menu_model_adapter_->CreateMenu();
    int run_types = views::MenuRunner::USE_ASH_SYS_UI_LAYOUT |
                    views::MenuRunner::CONTEXT_MENU |
                    views::MenuRunner::FIXED_ANCHOR;
    menu_runner_ =
        std::make_unique<views::MenuRunner>(std::move(menu), run_types);

    menu_runner_->RunMenuAt(source->GetWidget(), /*button_controller=*/nullptr,
                            source->GetBoundsInScreen(),
                            views::MenuAnchorPosition::kBubbleBottomLeft,
                            source_type, /*native_view_for_gestures=*/nullptr,
                            kTopRightNonRoundedCorners);
  }

 private:
  // Builds and saves SimpleMenuModel to `context_menu_model_`.
  void BuildMenuModel() {
    menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);

    menu_model_->AddItemWithIcon(
        CommandId::kAudioSettings,
        l10n_util::GetStringUTF16(
            IDS_ASH_VIDEO_CONFERENCE_SETTINGS_MENU_AUDIO_SETTINGS),
        ui::ImageModel::FromVectorIcon(kPrivacyIndicatorsMicrophoneIcon,
                                       cros_tokens::kCrosSysOnSurface,
                                       kIconSize));
    menu_model_->AddItemWithIcon(
        CommandId::kPrivacySettings,
        l10n_util::GetStringUTF16(
            IDS_ASH_VIDEO_CONFERENCE_SETTINGS_MENU_PRIVACY_SETTINGS),
        ui::ImageModel::FromVectorIcon(
            kSecurityIcon, cros_tokens::kCrosSysOnSurface, kIconSize));
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);

    // Adds Studio Look preference title. Creates an empty MenuItemView and
    // fills its content with custom style in
    // SettingsMenuModelAdapter::AppendMenuItem.
    menu_model_->AddTitle(std::u16string());

    menu_model_->AddCheckItem(
        CommandId::kPortraitRelighting,
        l10n_util::GetStringUTF16(
            IDS_ASH_VIDEO_CONFERENCE_SETTINGS_MENU_PORTRAIT_RELIGHTING));
    menu_model_->AddCheckItem(
        CommandId::kFaceRetouch,
        l10n_util::GetStringUTF16(
            IDS_ASH_VIDEO_CONFERENCE_SETTINGS_MENU_FACE_RETOUCH));
  }

  void OnMenuClosed() {
    menu_runner_.reset();
    menu_model_.reset();
    menu_model_adapter_.reset();
  }

  raw_ptr<CameraEffectsController> effects_controller_ =
      Shell::Get()->camera_effects_controller();
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuModelAdapter> menu_model_adapter_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
  base::OnceClosure close_bubble_callback_;
};

SettingsButton::SettingsButton(base::OnceClosure close_bubble_callback)
    : views::Button(base::BindRepeating(&SettingsButton::OnButtonActivated,
                                        base::Unretained(this))),
      context_menu_(
          std::make_unique<MenuController>(std::move(close_bubble_callback))) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout->SetOrientation(views::BoxLayout::Orientation::kHorizontal);

  AddChildView(views::Builder<views::ImageView>()
                   .SetImage(ui::ImageModel::FromVectorIcon(
                       kSystemMenuSettingsIcon, cros_tokens::kCrosSysOnSurface))
                   .SetImageSize(kIconSizeGfx)
                   .Build());
  AddChildView(views::Builder<views::ImageView>()
                   .SetImage(ui::ImageModel::FromVectorIcon(
                       kDropDownArrowIcon, cros_tokens::kCrosSysOnSurface))
                   .SetImageSize(kIconSizeGfx)
                   .Build());
  SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_VIDEO_CONFERENCE_SETTINGS_BUTTON_TOOLTIP));
  GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
      IDS_ASH_VIDEO_CONFERENCE_SETTINGS_BUTTON_TOOLTIP));
}

SettingsButton::~SettingsButton() = default;

void SettingsButton::OnButtonActivated(const ui::Event& event) {
  ui::MenuSourceType source_type;

  if (event.IsMouseEvent()) {
    source_type = ui::MENU_SOURCE_MOUSE;
  } else if (event.IsTouchEvent()) {
    source_type = ui::MENU_SOURCE_TOUCH;
  } else if (event.IsKeyEvent()) {
    source_type = ui::MENU_SOURCE_KEYBOARD;
  } else {
    source_type = ui::MENU_SOURCE_STYLUS;
  }

  context_menu_->ShowContextMenuForView(
      /*source=*/this, GetBoundsInScreen().CenterPoint(), source_type);
}

BEGIN_METADATA(SettingsButton)
END_METADATA

}  // namespace ash::video_conference
