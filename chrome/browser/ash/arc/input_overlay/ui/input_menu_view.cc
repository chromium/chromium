// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/input_menu_view.h"

#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/pill_button.h"
#include "ash/style/style_util.h"
#include "base/bind.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "net/base/url_util.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_types.h"

namespace arc {

namespace input_overlay {

namespace {
// Whole Menu measurements.
constexpr int kMenuWidth = 328;
constexpr int kMenuHeight = 244;

// Individual entries and header.
constexpr int kHeaderMinHeight = 64;
constexpr int kRowMinHeight = 60;

// Other misc sizes.
constexpr int kCloseButtonSize = 48;
constexpr int kCornerRadius = 16;
constexpr int kSideInset = 20;

// String styles/sizes.
constexpr char kGoogleSansFont[] = "Google Sans";
constexpr int kTitleFontSize = 20;
constexpr int kBodyFontSize = 13;

constexpr char kFeedbackUrl[] =
    "https://docs.google.com/forms/d/e/"
    "1FAIpQLSfL3ttPmopJj65P4EKr--SA18Sc9bbQVMnd0oueMhJu_42TbA/"
    "viewform?usp=pp_url";
// Entry for the survey form from above link.
constexpr char kGamePackageName[] = "entry.435412983";
constexpr char kBoardName[] = "entry.1492517074";
constexpr char kOsVersion[] = "entry.1961594320";

GURL GetAssembleUrl(DisplayOverlayController& controller) {
  GURL url(kFeedbackUrl);
  const auto* package_name = controller.GetPackageName();
  url = net::AppendQueryParameter(url, kGamePackageName, *package_name);
  url = net::AppendQueryParameter(url, kBoardName,
                                  base::SysInfo::HardwareModelName());
  url = net::AppendQueryParameter(url, kOsVersion,
                                  base::SysInfo::OperatingSystemVersion());
  return url;
}

}  // namespace

class InputMenuView::FeedbackButton : public views::LabelButton {
 public:
  explicit FeedbackButton(PressedCallback callback = PressedCallback(),
                          const std::u16string& text = std::u16string())
      : LabelButton(callback, text) {
    SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_MENU_SEND_FEEDBACK));
    SetBorder(views::CreateEmptyBorder(
        gfx::Insets::TLBR(0, kSideInset, 0, kSideInset)));
    label()->SetFontList(
        gfx::FontList({kGoogleSansFont}, gfx::Font::FontStyle::NORMAL,
                      kBodyFontSize, gfx::Font::Weight::NORMAL));

    auto* color_provider = ash::AshColorProvider::Get();
    DCHECK(color_provider);
    if (!color_provider)
      return;

    SetTextColor(
        views::Button::STATE_NORMAL,
        color_provider->GetContentLayerColor(
            ash::AshColorProvider::ContentLayerType::kTextColorPrimary));
    SetTextColor(
        views::Button::STATE_HOVERED,
        color_provider->GetContentLayerColor(
            ash::AshColorProvider::ContentLayerType::kTextColorPrimary));
    ash::StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                          /*highlight_on_hover=*/true,
                                          /*highlight_on_focus=*/true);
    SetMinSize(gfx::Size(0, kRowMinHeight));
  }

  FeedbackButton(const FeedbackButton&) = delete;
  FeedbackButton& operator=(const FeedbackButton&) = delete;
  ~FeedbackButton() override = default;
};

// static
std::unique_ptr<InputMenuView> InputMenuView::BuildMenuView(
    DisplayOverlayController* display_overlay_controller,
    views::View* entry_view) {
  // Ensure there is only one menu at any time.
  if (display_overlay_controller->HasMenuView())
    display_overlay_controller->RemoveInputMenuView();

  auto menu_view_ptr =
      std::make_unique<InputMenuView>(display_overlay_controller, entry_view);
  menu_view_ptr->Init();

  return menu_view_ptr;
}

InputMenuView::InputMenuView(
    DisplayOverlayController* display_overlay_controller,
    views::View* entry_view)
    : entry_view_(entry_view),
      display_overlay_controller_(display_overlay_controller) {}

InputMenuView::~InputMenuView() {}

void InputMenuView::CloseMenu() {
  if (display_overlay_controller_)
    display_overlay_controller_->SetDisplayMode(DisplayMode::kView);
}

void InputMenuView::Init() {
  DCHECK(display_overlay_controller_);
  DCHECK(ash::AshColorProvider::Get());
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  auto* color_provider = ash::AshColorProvider::Get();
  auto bg_color = color_provider->GetBackgroundColorInMode(
      color_provider->IsDarkModeEnabled());
  SetBackground(views::CreateRoundedRectBackground(bg_color, kCornerRadius));
  SkColor color = color_provider->GetContentLayerColor(
      ash::AshColorProvider::ContentLayerType::kTextColorPrimary);

  {
    // Title, main control for the feature and close button.
    auto header_view = std::make_unique<views::View>();
    header_view->SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kHorizontal)
        .SetCrossAxisAlignment(views::LayoutAlignment::kStretch);

    auto* menu_title = ash::login_views_utils::CreateBubbleLabel(
        l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_MENU_GAME_CONTROL),
        /*view_defining_max_width=*/nullptr, color,
        /*font_list=*/
        gfx::FontList({kGoogleSansFont}, gfx::Font::FontStyle::NORMAL,
                      kTitleFontSize, gfx::Font::Weight::MEDIUM),
        /*line_height=*/kHeaderMinHeight);
    header_view->AddChildView(std::move(menu_title));

    game_control_toggle_ =
        header_view->AddChildView(std::make_unique<views::ToggleButton>(
            base::BindRepeating(&InputMenuView::OnToggleGameControlPressed,
                                base::Unretained(this))));
    game_control_toggle_->SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_MENU_GAME_CONTROL));
    game_control_toggle_->SetIsOn(
        display_overlay_controller_->GetTouchInjectorEnable());

    auto close_icon = gfx::CreateVectorIcon(vector_icons::kCloseIcon, color);
    auto close_button = std::make_unique<views::ImageButton>(
        base::BindRepeating(&InputMenuView::CloseMenu, base::Unretained(this)));
    close_button->SetImage(views::Button::STATE_NORMAL, close_icon);
    close_button->SetBackground(
        views::CreateSolidBackground(SK_ColorTRANSPARENT));
    close_button->SetSize(gfx::Size(kCloseButtonSize, kCloseButtonSize));
    close_button->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
    close_button->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
    // TODO(djacobo): Pick a proper size close button.
    close_button->SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_NOTIFICATION_BUTTON_CLOSE));
    close_button_ = header_view->AddChildView(std::move(close_button));
    menu_title->SetBorder(views::CreateEmptyBorder(CalculateInsets(
        header_view.get(), /*left=*/20, /*right=*/8, /*other_spacing=*/16)));
    game_control_toggle_->SetBorder(
        views::CreateEmptyBorder(gfx::Insets::TLBR(0, 0, 0, 16)));

    AddChildView(std::move(header_view));
    AddChildView(BuildSeparator());
  }
  {
    // Key binding customization button.
    auto customize_view = std::make_unique<views::View>();
    customize_view->SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kHorizontal)
        .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

    auto* key_mapping_label = ash::login_views_utils::CreateBubbleLabel(
        l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_MENU_KEY_MAPPING),
        /*view_defining_max_width=*/nullptr, color,
        /*font_list=*/
        gfx::FontList({kGoogleSansFont}, gfx::Font::FontStyle::NORMAL,
                      kBodyFontSize, gfx::Font::Weight::NORMAL),
        /*line_height=*/kRowMinHeight);
    customize_view->AddChildView(std::move(key_mapping_label));

    customize_button_ =
        customize_view->AddChildView(std::make_unique<ash::PillButton>(
            base::BindRepeating(&InputMenuView::OnButtonCustomizedPressed,
                                base::Unretained(this)),
            l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_MENU_CUSTOMIZE_BUTTON),
            ash::PillButton::Type::kIconless,
            /*icon=*/nullptr));
    key_mapping_label->SetBorder(views::CreateEmptyBorder(
        CalculateInsets(customize_view.get(), /*left=*/kSideInset,
                        /*right=*/kSideInset, /*other_spacing=*/0)));
    AddChildView(std::move(customize_view));
    AddChildView(BuildSeparator());
  }
  {
    // Hint label and toggle.
    auto hint_view = std::make_unique<views::View>();
    hint_view->SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kHorizontal)
        .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

    auto* hint_label = ash::login_views_utils::CreateBubbleLabel(
        l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_MENU_SHOW_HINT_OVERLAY),
        /*view_defining_max_width=*/nullptr, color,
        /*font_list=*/
        gfx::FontList({kGoogleSansFont}, gfx::Font::FontStyle::NORMAL,
                      kBodyFontSize, gfx::Font::Weight::NORMAL),
        /*line_height=*/kRowMinHeight);
    hint_view->AddChildView(hint_label);
    show_hint_toggle_ = hint_view->AddChildView(
        std::make_unique<views::ToggleButton>(base::BindRepeating(
            &InputMenuView::OnToggleShowHintPressed, base::Unretained(this))));
    show_hint_toggle_->SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_MENU_SHOW_HINT_OVERLAY));
    show_hint_toggle_->SetIsOn(
        display_overlay_controller_->GetInputMappingViewVisible());
    hint_label->SetBorder(views::CreateEmptyBorder(
        CalculateInsets(hint_view.get(), /*left=*/kSideInset,
                        /*right=*/kSideInset, /*other_spacing=*/0)));
    AddChildView(std::move(hint_view));
    AddChildView(BuildSeparator());
  }
  {
    // Feedback row.
    auto feedback_row_view = std::make_unique<views::View>();
    feedback_row_view->SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kHorizontal)
        .SetMainAxisAlignment(views::LayoutAlignment::kStart)
        .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

    auto feedback_button = std::make_unique<FeedbackButton>(
        base::BindRepeating(&InputMenuView::OnButtonSendFeedbackPressed,
                            base::Unretained(this)),
        l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_MENU_SEND_FEEDBACK));
    AddChildView(std::move(feedback_button));
  }

  SetSize(gfx::Size(kMenuWidth, kMenuHeight));
  SetPosition(gfx::Point(entry_view_->x() - width() + entry_view_->width(),
                         entry_view_->y()));
}

std::unique_ptr<views::View> InputMenuView::BuildSeparator() {
  auto separator = std::make_unique<views::Separator>();
  separator->SetColor(SK_ColorGRAY);

  return std::move(separator);
}

void InputMenuView::OnToggleGameControlPressed() {
  DCHECK(display_overlay_controller_);
  if (!display_overlay_controller_)
    return;
  display_overlay_controller_->SetTouchInjectorEnable(
      game_control_toggle_->GetIsOn());
  // Also show/hide the input mapping when enable/disable the game controller.
  show_hint_toggle_->SetIsOn(game_control_toggle_->GetIsOn());
  display_overlay_controller_->SetInputMappingVisible(
      show_hint_toggle_->GetIsOn());
}

void InputMenuView::OnToggleShowHintPressed() {
  DCHECK(display_overlay_controller_);
  display_overlay_controller_->SetInputMappingVisible(
      show_hint_toggle_->GetIsOn());
}

void InputMenuView::OnButtonCustomizedPressed() {
  DCHECK(display_overlay_controller_);
  if (!display_overlay_controller_)
    return;
  // Change display mode, load edit UI per action and overall edit buttons.
  display_overlay_controller_->SetDisplayMode(DisplayMode::kEdit);
}

void InputMenuView::OnButtonSendFeedbackPressed() {
  DCHECK(display_overlay_controller_);
  if (!display_overlay_controller_)
    return;

  GURL url = GetAssembleUrl(*display_overlay_controller_);
  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      url, ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction);
}

gfx::Insets InputMenuView::CalculateInsets(views::View* view,
                                           int left,
                                           int right,
                                           int other_spacing) const {
  int total_width = 0;
  for (auto* child : view->children())
    total_width += child->GetPreferredSize().width();

  int right_inset =
      std::max(0, kMenuWidth - (total_width + left + right + other_spacing));
  return gfx::Insets::TLBR(0, left, 0, right_inset);
}

}  // namespace input_overlay

}  // namespace arc
