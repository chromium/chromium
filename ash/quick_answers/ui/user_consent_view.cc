// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_answers/ui/user_consent_view.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/quick_answers/quick_answers_ui_controller.h"
#include "ash/quick_answers/ui/quick_answers_pre_target_handler.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace quick_answers {

namespace {

// Main view (or common) specs.
constexpr int kMarginDip = 10;
constexpr int kLineHeightDip = 20;
constexpr int kContentSpacingDip = 8;
constexpr gfx::Insets kMainViewInsets = {16, 12, 16, 16};
constexpr gfx::Insets kContentInsets = {0, 12, 0, 0};
constexpr SkColor kMainViewBgColor = SK_ColorWHITE;

// Assistant icon.
constexpr int kAssistantIconSizeDip = 16;

// Title text.
constexpr SkColor kTitleTextColor = gfx::kGoogleGrey900;
constexpr int kTitleFontSizeDelta = 2;

// Description text.
constexpr SkColor kDescTextColor = gfx::kGoogleGrey700;
constexpr int kDescFontSizeDelta = 1;

// Buttons common.
constexpr int kButtonSpacingDip = 8;
constexpr gfx::Insets kButtonBarInsets = {8, 0, 0, 0};
constexpr gfx::Insets kButtonInsets = {6, 16, 6, 16};
constexpr int kButtonFontSizeDelta = 1;

// Manage-Settings button.
constexpr SkColor kSettingsButtonTextColor = gfx::kGoogleBlue600;
constexpr char kA11ySettingsButtonDescText[] =
    "Click to open Google Assistant settings.";

// Grant-Consent button.
constexpr SkColor kConsentButtonTextColor = gfx::kGoogleGrey200;
constexpr char kA11yConsentButtonDescText[] =
    "Let Assistant show info such as definition or unit conversion for your "
    "selection.";

// Dogfood button.
constexpr int kDogfoodButtonMarginDip = 4;
constexpr int kDogfoodButtonSizeDip = 20;
constexpr SkColor kDogfoodButtonColor = gfx::kGoogleGrey500;

// Accessibility.
// TODO(siabhijeet): Move to grd after finalizing with UX.
constexpr char kA11yInfoAlertText[] =
    "New feature available, use Up arrow key to learn more.";
constexpr char kA11yInfoDescTemplate[] =
    "%s Use Left or Right arrow keys to manage this feature.";

// Create and return a simple label with provided specs.
std::unique_ptr<views::Label> CreateLabel(const base::string16& text,
                                          const SkColor color,
                                          int font_size_delta) {
  auto label = std::make_unique<views::Label>(text);
  label->SetAutoColorReadabilityEnabled(false);
  label->SetEnabledColor(color);
  label->SetLineHeight(kLineHeightDip);
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label->SetFontList(
      views::Label::GetDefaultFontList().DeriveWithSizeDelta(font_size_delta));
  return label;
}

// views::LabelButton with custom line-height, color and font-list for the
// underlying label.
class CustomizedLabelButton : public views::MdTextButton {
 public:
  CustomizedLabelButton(views::ButtonListener* listener,
                        const base::string16& text,
                        const SkColor color)
      : MdTextButton(listener, text) {
    SetCustomPadding(kButtonInsets);
    SetEnabledTextColors(color);
    label()->SetLineHeight(kLineHeightDip);
    label()->SetFontList(views::Label::GetDefaultFontList()
                             .DeriveWithSizeDelta(kButtonFontSizeDelta)
                             .DeriveWithWeight(gfx::Font::Weight::MEDIUM));
  }

  // Disallow copy and assign.
  CustomizedLabelButton(const CustomizedLabelButton&) = delete;
  CustomizedLabelButton& operator=(const CustomizedLabelButton&) = delete;

  ~CustomizedLabelButton() override = default;

  // views::View:
  const char* GetClassName() const override { return "CustomizedLabelButton"; }
};

}  // namespace

// UserConsentView -------------------------------------------------------------

UserConsentView::UserConsentView(const gfx::Rect& anchor_view_bounds,
                                 const base::string16& intent_type,
                                 const base::string16& intent_text,
                                 QuickAnswersUiController* ui_controller)
    : anchor_view_bounds_(anchor_view_bounds),
      event_handler_(std::make_unique<QuickAnswersPreTargetHandler>(this)),
      ui_controller_(ui_controller),
      focus_search_(std::make_unique<QuickAnswersFocusSearch>(
          this,
          base::BindRepeating(&UserConsentView::GetFocusableViews,
                              base::Unretained(this)))) {
  if (intent_type.empty() || intent_text.empty()) {
    title_ = l10n_util::GetStringUTF16(
        IDS_ASH_QUICK_ANSWERS_USER_CONSENT_VIEW_TITLE_TEXT);
  } else {
    title_ = l10n_util::GetStringFUTF16(
        IDS_ASH_QUICK_ANSWERS_USER_CONSENT_VIEW_TITLE_TEXT_WITH_INTENT,
        intent_type, intent_text);
  }

  InitLayout();
  InitWidget();

  // Focus should cycle to each of the buttons the view contains and back to it.
  SetFocusBehavior(FocusBehavior::ALWAYS);
  views::FocusRing::Install(this);

  // Allow tooltips to be shown despite menu-controller owning capture.
  GetWidget()->SetNativeWindowProperty(
      views::TooltipManager::kGroupingPropertyKey,
      reinterpret_cast<void*>(views::MenuConfig::kMenuControllerGroupingId));

  // Read out user-consent notice if screen-reader is active.
  GetViewAccessibility().AnnounceText(base::UTF8ToUTF16(kA11yInfoAlertText));
}

UserConsentView::~UserConsentView() = default;

const char* UserConsentView::GetClassName() const {
  return "UserConsentView";
}

gfx::Size UserConsentView::CalculatePreferredSize() const {
  // View should match width of the anchor.
  auto width = anchor_view_bounds_.width();
  return gfx::Size(width, GetHeightForWidth(width));
}

void UserConsentView::OnFocus() {
  // Unless screen-reader mode is enabled, transfer the focus to an actionable
  // button, otherwise retain to read out its contents.
  if (!ash::Shell::Get()
           ->accessibility_controller()
           ->spoken_feedback()
           .enabled())
    settings_button_->RequestFocus();
}

views::FocusTraversable* UserConsentView::GetPaneFocusTraversable() {
  return focus_search_.get();
}

void UserConsentView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kDialog;
  node_data->SetName(title_);
  auto desc =
      base::StringPrintf(kA11yInfoDescTemplate,
                         l10n_util::GetStringUTF8(
                             IDS_ASH_QUICK_ANSWERS_USER_CONSENT_VIEW_DESC_TEXT)
                             .c_str());
  node_data->SetDescription(desc);
}

std::vector<views::View*> UserConsentView::GetFocusableViews() {
  std::vector<views::View*> focusable_views;
  // The view itself is not included in focus loop, unless screen-reader is on.
  if (ash::Shell::Get()
          ->accessibility_controller()
          ->spoken_feedback()
          .enabled()) {
    focusable_views.push_back(this);
  }
  focusable_views.push_back(settings_button_);
  focusable_views.push_back(consent_button_);
  if (dogfood_button_)
    focusable_views.push_back(dogfood_button_);
  return focusable_views;
}

void UserConsentView::ButtonPressed(views::Button* sender,
                                    const ui::Event& event) {
  if (sender == consent_button_) {
    // When user-consent is acknowledged, QuickAnswersView will be displayed
    // instead of dismissing the menu.
    event_handler_->set_dismiss_anchor_menu_on_view_closed(false);
    ui_controller_->OnConsentGrantedButtonPressed();
    return;
  }
  if (sender == settings_button_) {
    ui_controller_->OnManageSettingsButtonPressed();
    return;
  }
  if (sender == dogfood_button_) {
    ui_controller_->OnDogfoodButtonPressed();
    return;
  }
}

void UserConsentView::UpdateAnchorViewBounds(
    const gfx::Rect& anchor_view_bounds) {
  anchor_view_bounds_ = anchor_view_bounds;
  UpdateWidgetBounds();
}

void UserConsentView::InitLayout() {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetBackground(views::CreateSolidBackground(kMainViewBgColor));

  // Main-view Layout.
  main_view_ = AddChildView(std::make_unique<views::View>());
  auto* layout =
      main_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, kMainViewInsets));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  // Assistant icon.
  auto* assistant_icon =
      main_view_->AddChildView(std::make_unique<views::ImageView>());
  assistant_icon->SetBorder(views::CreateEmptyBorder(
      (kLineHeightDip - kAssistantIconSizeDip) / 2, 0, 0, 0));
  assistant_icon->SetImage(gfx::CreateVectorIcon(
      chromeos::kAssistantIcon, kAssistantIconSizeDip, gfx::kPlaceholderColor));

  // Content.
  InitContent();

  // Add dogfood icon, if in dogfood.
  if (chromeos::features::IsQuickAnswersDogfood())
    AddDogfoodButton();
}

void UserConsentView::InitContent() {
  // Layout.
  content_ = main_view_->AddChildView(std::make_unique<views::View>());
  content_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kContentInsets,
      kContentSpacingDip));

  // Title.
  content_->AddChildView(
      CreateLabel(title_, kTitleTextColor, kTitleFontSizeDelta));

  // Description.
  auto* desc = content_->AddChildView(
      CreateLabel(l10n_util::GetStringUTF16(
                      IDS_ASH_QUICK_ANSWERS_USER_CONSENT_VIEW_DESC_TEXT),
                  kDescTextColor, kDescFontSizeDelta));
  desc->SetMultiLine(true);
  // BoxLayout does not necessarily size the height of multi-line labels
  // properly (crbug/682266). The label is thus explicitly sized to the width
  // (and height) it would need to be for the UserConsentView to be the same
  // width as the anchor, so its preferred size will be calculated correctly.
  int desc_desired_width = anchor_view_bounds_.width() -
                           kMainViewInsets.width() - kContentInsets.width() -
                           kAssistantIconSizeDip;
  desc->SizeToFit(desc_desired_width);

  // Button bar.
  InitButtonBar();
}

void UserConsentView::InitButtonBar() {
  // Layout.
  auto* button_bar = content_->AddChildView(std::make_unique<views::View>());
  auto* layout =
      button_bar->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, kButtonBarInsets,
          kButtonSpacingDip));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kEnd);

  // Manage-Settings button.
  auto settings_button = std::make_unique<CustomizedLabelButton>(
      this,
      l10n_util::GetStringUTF16(
          IDS_ASH_QUICK_ANSWERS_USER_CONSENT_VIEW_MANAGE_SETTINGS_BUTTON),
      kSettingsButtonTextColor);
  settings_button->GetViewAccessibility().OverrideDescription(
      kA11ySettingsButtonDescText);
  settings_button_ = button_bar->AddChildView(std::move(settings_button));

  // Grant-Consent button.
  auto consent_button = std::make_unique<CustomizedLabelButton>(
      this,
      l10n_util::GetStringUTF16(
          IDS_ASH_QUICK_ANSWERS_USER_CONSENT_VIEW_GRANT_CONSENT_BUTTON),
      kConsentButtonTextColor);
  consent_button->SetProminent(true);
  consent_button->GetViewAccessibility().OverrideDescription(
      kA11yConsentButtonDescText);
  consent_button_ = button_bar->AddChildView(std::move(consent_button));
}

void UserConsentView::InitWidget() {
  views::Widget::InitParams params;
  params.activatable = views::Widget::InitParams::Activatable::ACTIVATABLE_NO;
  params.shadow_elevation = 2;
  params.shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;

  // Parent the widget depending on the context.
  auto* active_menu_controller = views::MenuController::GetActiveInstance();
  if (active_menu_controller && active_menu_controller->owner()) {
    params.parent = active_menu_controller->owner()->GetNativeView();
    params.child = true;
  } else {
    params.context = Shell::Get()->GetRootWindowForNewWindows();
  }

  views::Widget* widget = new views::Widget();
  widget->Init(std::move(params));
  widget->SetContentsView(this);
  UpdateWidgetBounds();
}

void UserConsentView::AddDogfoodButton() {
  auto* dogfood_view = AddChildView(std::make_unique<views::View>());
  auto* layout =
      dogfood_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical,
          gfx::Insets(kDogfoodButtonMarginDip)));
  layout->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kEnd);
  auto dogfood_button = std::make_unique<views::ImageButton>(/*listener=*/this);
  dogfood_button->SetImage(
      views::Button::ButtonState::STATE_NORMAL,
      gfx::CreateVectorIcon(kDogfoodIcon, kDogfoodButtonSizeDip,
                            kDogfoodButtonColor));
  dogfood_button->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_QUICK_ANSWERS_DOGFOOD_BUTTON_TOOLTIP_TEXT));
  dogfood_button->SetFocusForPlatform();
  dogfood_button_ = dogfood_view->AddChildView(std::move(dogfood_button));
}

void UserConsentView::UpdateWidgetBounds() {
  const gfx::Size size = GetPreferredSize();
  int x = anchor_view_bounds_.x();
  int y = anchor_view_bounds_.y() - size.height() - kMarginDip;
  if (y < display::Screen::GetScreen()
              ->GetDisplayMatching(anchor_view_bounds_)
              .bounds()
              .y()) {
    y = anchor_view_bounds_.bottom() + kMarginDip;
  }
  gfx::Rect bounds({x, y}, size);
  wm::ConvertRectFromScreen(GetWidget()->GetNativeWindow()->parent(), &bounds);
  GetWidget()->SetBounds(bounds);
}

}  // namespace quick_answers
}  // namespace ash
