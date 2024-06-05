// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/input_menu_view.h"

#include <utility>

#include "ash/components/arc/compat_mode/style/arc_color_provider.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/pill_button.h"
#include "ash/style/style_util.h"
#include "base/functional/bind.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/arc/input_overlay/arc_input_overlay_metrics.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "chrome/grit/generated_resources.h"
#include "net/base/url_util.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_types.h"
#include "ui/chromeos/styles/cros_styles.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
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
#include "ui/views/vector_icons.h"

namespace arc::input_overlay {

namespace {
// If the parent's width smaller than `kParentWidthThreshold`, it uses smaller
// specs.
constexpr int kParentWidthThreshold = 376;
// Whole Menu measurements.
constexpr int kMenuWidth = 328;
constexpr int kMenuWidthSmall = 280;
constexpr int kMenuHeight = 244;
constexpr int kMenuMarginSmall = 24;

// Individual entries and header.
constexpr int kHeaderMinHeight = 64;
constexpr int kRowMinHeight = 60;

// Other misc sizes.
constexpr int kCloseButtonSize = 24;
constexpr int kCloseButtonSide = 12;
constexpr int kCloseButtonLeftSide = 8;
constexpr int kCornerRadius = 16;
constexpr int kSideInset = 20;
constexpr int kToggleInset = 16;
constexpr int kCloseInset = 8;

// String styles/sizes.
constexpr char kGoogleSansFont[] = "Google Sans";
constexpr int kTitleFontSize = 20;
constexpr int kTitleFontSizeSmall = 16;
constexpr int kBodyFontSize = 13;

// About Alpha style.
constexpr int kAlphaFontSize = 10;
constexpr int kAlphaCornerRadius = 4;
constexpr int kAlphaHeight = 16;
constexpr int kAlphaSidePadding = 4;
constexpr int kAlphaLeftMargin = 8;
constexpr int kAlphaLeftMarginSmall = 4;

constexpr char kFeedbackUrl[] =
    "https://docs.google.com/forms/d/e/"
    "1FAIpQLSfL3ttPmopJj65P4EKr--SA18Sc9bbQVMnd0oueMhJu_42TbA/"
    "viewform?usp=pp_url";
// Entry for the survey form from above link.
constexpr char kGamePackageName[] = "entry.435412983";
constexpr char kBoardName[] = "entry.1492517074";
constexpr char kOsVersion[] = "entry.1961594320";

// Pass `package_name` by value because the focus will be changed to the
// browser.
GURL GetAssembleUrl(std::string package_name) {
  GURL url(kFeedbackUrl);
  url = net::AppendQueryParameter(url, kGamePackageName, package_name);
  url = net::AppendQueryParameter(url, kBoardName,
                                  base::SysInfo::HardwareModelName());
  url = net::AppendQueryParameter(url, kOsVersion,
                                  base::SysInfo::OperatingSystemVersion());
  return url;
}

int GetMenuWidth(int parent_width) {
  return parent_width < kParentWidthThreshold ? kMenuWidthSmall : kMenuWidth;
}

int GetTitleFontSize(int parent_width) {
  return parent_width < kParentWidthThreshold ? kTitleFontSizeSmall
                                              : kTitleFontSize;
}

int GetAlphaLeftMargin(int parent_width) {
  return parent_width < kParentWidthThreshold ? kAlphaLeftMarginSmall
                                              : kAlphaLeftMargin;
}

}  // namespace

class InputMenuView::FeedbackButton : public views::LabelButton {
  METADATA_HEADER(FeedbackButton, views::LabelButton)

 public:
  explicit FeedbackButton(PressedCallback callback = PressedCallback(),
                          const std::u16string& text = std::u16string())
      : LabelButton(std::move(callback), text) {
    GetViewAccessibility().SetName(
        l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_MENU_SEND_FEEDBACK));
    SetBorder(views::CreateEmptyBorder(
        gfx::Insets::TLBR(0, kSideInset, 0, kSideInset)));
    label()->SetFontList(
        gfx::FontList({kGoogleSansFont}, gfx::Font::FontStyle::NORMAL,
                      kBodyFontSize, gfx::Font::Weight::NORMAL));

    auto* color_provider = ash::AshColorProvider::Get();
    DCHECK(color_provider);
    if (!color_provider) {
      return;
    }

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

BEGIN_METADATA(InputMenuView, FeedbackButton)
END_METADATA

// static
std::unique_ptr<InputMenuView> InputMenuView::BuildMenuView(
    DisplayOverlayController* display_overlay_controller,
    views::View* entry_view,
    const gfx::Size& parent_size) {
  // Ensure there is only one menu at any time.
  if (display_overlay_controller->HasMenuView()) {
    display_overlay_controller->RemoveInputMenuView();
  }

  auto menu_view_ptr =
      std::make_unique<InputMenuView>(display_overlay_controller, entry_view);
  menu_view_ptr->Init(parent_size);

  return menu_view_ptr;
}

InputMenuView::InputMenuView(
    DisplayOverlayController* display_overlay_controller,
    views::View* entry_view)
    : entry_view_(entry_view),
      display_overlay_controller_(display_overlay_controller) {}

InputMenuView::~InputMenuView() {}

void InputMenuView::OnThemeChanged() {
  views::View::OnThemeChanged();
  const auto bg_color = GetColorProvider()->GetColor(cros_tokens::kBgColor);
  SetBackground(views::CreateRoundedRectBackground(bg_color, kCornerRadius));
}

void InputMenuView::CloseMenu() {
  if (display_overlay_controller_) {
    display_overlay_controller_->SetDisplayModeAlpha(DisplayMode::kView);
  }
}

void InputMenuView::Init(const gfx::Size& parent_size) {
  DCHECK(display_overlay_controller_);
  DCHECK(ash::AshColorProvider::Get());
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  auto* color_provider = ash::AshColorProvider::Get();
  SkColor color = color_provider->GetContentLayerColor(
      ash::AshColorProvider::ContentLayerType::kTextColorPrimary);
  int menu_width = GetMenuWidth(parent_size.width());
  {
    // Title, main control for the feature and close button.
    auto header_view = std::make_unique<views::View>();
    header_view->SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kHorizontal)
        .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

    auto* menu_title =
        header_view->AddChildView(ash::login_views_utils::CreateBubbleLabel(
            l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_GAME_CONTROLS_ALPHA),
            /*view_defining_max_width=*/nullptr, color,
            /*font_list=*/
            gfx::FontList({kGoogleSansFont}, gfx::Font::FontStyle::NORMAL,
                          GetTitleFontSize(parent_size.width()),
                          gfx::Font::Weight::MEDIUM),
            /*line_height=*/kHeaderMinHeight));

    auto* alpha_label = header_view->AddChildView(
        ash::login_views_utils::CreateThemedBubbleLabel(
            l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_RELEASE_ALPHA),
            /*view_defining_max_width=*/nullptr,
            /*enabled_color_type=*/cros_tokens::kCrosSysPrimary,
            gfx::FontList({ash::login_views_utils::kGoogleSansFont},
                          gfx::Font::FontStyle::NORMAL, kAlphaFontSize,
                          gfx::Font::Weight::MEDIUM)));
    alpha_label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    alpha_label->SetPreferredSize(gfx::Size(
        alpha_label->GetPreferredSize().width() + 2 * kAlphaSidePadding,
        kAlphaHeight));
    alpha_label->SetBackground(views::CreateThemedRoundedRectBackground(
        cros_tokens::kCrosSysHighlightShape, kAlphaCornerRadius));

    game_control_toggle_ =
        header_view->AddChildView(std::make_unique<views::ToggleButton>(
            base::BindRepeating(&InputMenuView::OnToggleGameControlPressed,
                                base::Unretained(this))));
    game_control_toggle_->GetViewAccessibility().SetName(
        l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_GAME_CONTROLS_ALPHA));
    game_control_toggle_->SetIsOn(
        display_overlay_controller_->GetTouchInjectorEnable());

    auto close_icon = ui::ImageModel::FromVectorIcon(views::kIcCloseIcon, color,
                                                     kCloseButtonSize);
    auto close_button = std::make_unique<views::ImageButton>(
        base::BindRepeating(&InputMenuView::CloseMenu, base::Unretained(this)));
    close_button->SetImageModel(views::Button::STATE_NORMAL, close_icon);
    close_button->SetBackground(
        views::CreateSolidBackground(SK_ColorTRANSPARENT));
    close_button->SetBorder(views::CreateEmptyBorder(
        gfx::Insets::TLBR(kCloseButtonSide, kCloseButtonLeftSide,
                          kCloseButtonSide, kCloseButtonSide)));
    close_button->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
    close_button->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
    const auto button_name =
        l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_ACCESSIBILITY_ALPHA);
    close_button->GetViewAccessibility().SetName(button_name);
    close_button->SetTooltipText(button_name);
    close_button_ = header_view->AddChildView(std::move(close_button));
    menu_title->SetBorder(
        views::CreateEmptyBorder(gfx::Insets::TLBR(0, kSideInset, 0, 0)));
    game_control_toggle_->SetBorder(
        views::CreateEmptyBorder(gfx::Insets::TLBR(0, 0, 0, kToggleInset)));
    SetCustomToggleColor(game_control_toggle_);
    alpha_label->SetProperty(
        views::kMarginsKey,
        CalculateInsets(header_view.get(),
                        GetAlphaLeftMargin(parent_size.width()), /*right=*/0,
                        /*other_spacing=*/kCloseInset, menu_width));

    AddChildView(std::move(header_view));
    AddChildView(BuildSeparator());
  }
  {
    // Key binding customization button.
    auto customize_view = std::make_unique<views::View>();
    customize_view->SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kHorizontal)
        .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

    auto* key_mapping_label =
        customize_view->AddChildView(ash::login_views_utils::CreateBubbleLabel(
            l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_MENU_KEY_MAPPING),
            /*view_defining_max_width=*/nullptr, color,
            /*font_list=*/
            gfx::FontList({kGoogleSansFont}, gfx::Font::FontStyle::NORMAL,
                          kBodyFontSize, gfx::Font::Weight::NORMAL),
            /*line_height=*/kRowMinHeight));

    edit_button_ =
        customize_view->AddChildView(std::make_unique<ash::PillButton>(
            base::BindRepeating(&InputMenuView::OnEditButtonPressed,
                                base::Unretained(this)),
            l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_MENU_EDIT_BUTTON),
            ash::PillButton::Type::kDefaultWithoutIcon,
            /*icon=*/nullptr));
    edit_button_->SetEnabled(game_control_toggle_->GetIsOn());
    key_mapping_label->SetBorder(views::CreateEmptyBorder(CalculateInsets(
        customize_view.get(), /*left=*/kSideInset,
        /*right=*/kSideInset, /*other_spacing=*/0, menu_width)));
    AddChildView(std::move(customize_view));
    AddChildView(BuildSeparator());
  }
  {
    // Hint label and toggle.
    auto hint_view = std::make_unique<views::View>();
    hint_view->SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kHorizontal)
        .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

    auto* mapping_label =
        hint_view->AddChildView(ash::login_views_utils::CreateBubbleLabel(
            l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_MENU_SHOW_KEY_MAPPING),
            /*view_defining_max_width=*/nullptr, color,
            /*font_list=*/
            gfx::FontList({kGoogleSansFont}, gfx::Font::FontStyle::NORMAL,
                          kBodyFontSize, gfx::Font::Weight::NORMAL),
            /*line_height=*/kRowMinHeight));
    show_mapping_toggle_ = hint_view->AddChildView(
        std::make_unique<views::ToggleButton>(base::BindRepeating(
            &InputMenuView::OnToggleShowHintPressed, base::Unretained(this))));
    show_mapping_toggle_->GetViewAccessibility().SetName(
        l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_MENU_SHOW_KEY_MAPPING));
    show_mapping_toggle_->SetEnabled(game_control_toggle_->GetIsOn());
    show_mapping_toggle_->SetIsOn(
        game_control_toggle_->GetIsOn() &&
        display_overlay_controller_->GetInputMappingViewVisible());
    SetCustomToggleColor(show_mapping_toggle_);
    mapping_label->SetBorder(views::CreateEmptyBorder(CalculateInsets(
        hint_view.get(), /*left=*/kSideInset,
        /*right=*/kSideInset, /*other_spacing=*/0, menu_width)));
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

  SetSize(gfx::Size(menu_width, kMenuHeight));
  int x;
  int y = entry_view_->y();

  x = entry_view_->x();
  // If the menu entry view is on the right side of the screen, bias toward
  // the center.
  if (x > parent_size.width() / 2) {
    x -= width() - entry_view_->width();
  }
  // Set the menu at the middle if there is not enough margin on the right
  // or left side.
  if (x + width() > parent_size.width() || x < 0) {
    x = std::max(0, parent_size.width() - width() - kMenuMarginSmall);
  }

  // If the menu entry is at the bottom side of the screen, bias towards the
  // center.
  if (y > parent_size.height() / 2) {
    y -= height() - entry_view_->height();
  }

  // Set the menu at the bottom if there is not enough margin on the bottom
  // side.
  if (y + height() > parent_size.height()) {
    y = std::max(0, parent_size.height() - height() - kMenuMarginSmall);
  }

  SetPosition(gfx::Point(x, y));
}

std::unique_ptr<views::View> InputMenuView::BuildSeparator() {
  auto separator = std::make_unique<views::Separator>();
  separator->SetColorId(ui::kColorAshSystemUIMenuSeparator);

  return std::move(separator);
}

void InputMenuView::OnToggleGameControlPressed() {
  DCHECK(display_overlay_controller_);
  if (!display_overlay_controller_) {
    return;
  }
  const bool enabled = game_control_toggle_->GetIsOn();
  display_overlay_controller_->SetTouchInjectorEnable(enabled);
  // Adjust `enabled_` and `visible_` properties to match `Game controls`.
  show_mapping_toggle_->SetIsOn(enabled);
  display_overlay_controller_->SetInputMappingVisible(
      /*visible=*/enabled, /*store_visible_state=*/true);
  show_mapping_toggle_->SetEnabled(enabled);
  edit_button_->SetEnabled(enabled);
}

void InputMenuView::OnToggleShowHintPressed() {
  DCHECK(display_overlay_controller_);
  display_overlay_controller_->SetInputMappingVisible(
      /*visible=*/show_mapping_toggle_->GetIsOn(),
      /*store_visible_state=*/true);
}

void InputMenuView::OnEditButtonPressed() {
  DCHECK(display_overlay_controller_);
  if (!display_overlay_controller_) {
    return;
  }
  // Force key-binding labels ON before entering edit mode.
  if (!show_mapping_toggle_->GetIsOn()) {
    show_mapping_toggle_->SetIsOn(true);
    display_overlay_controller_->SetInputMappingVisible(/*visible=*/true);
  }
  RecordInputOverlayCustomizedUsage(
      display_overlay_controller_->GetPackageName());
  // Change display mode, load edit UI per action and overall edit buttons; make
  // sure the following line is at the bottom because edit mode will kill this
  // view.
  display_overlay_controller_->SetDisplayModeAlpha(DisplayMode::kEdit);
}

void InputMenuView::OnButtonSendFeedbackPressed() {
  DCHECK(display_overlay_controller_);
  if (!display_overlay_controller_) {
    return;
  }

  GURL url = GetAssembleUrl(display_overlay_controller_->GetPackageName());
  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      url, ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kNewForegroundTab);
}

gfx::Insets InputMenuView::CalculateInsets(views::View* view,
                                           int left,
                                           int right,
                                           int other_spacing,
                                           int menu_width) const {
  int total_width = 0;
  for (views::View* child : view->children()) {
    total_width += child->GetPreferredSize().width();
  }

  int right_inset =
      std::max(0, menu_width - (total_width + left + right + other_spacing));
  return gfx::Insets::TLBR(0, left, 0, right_inset);
}

void InputMenuView::SetCustomToggleColor(views::ToggleButton* toggle) {
  if (const auto* color_provider = ash::AshColorProvider::Get()) {
    toggle->SetThumbOnColor(color_provider->GetContentLayerColor(
        ash::AshColorProvider::ContentLayerType::kSwitchKnobColorActive));
    toggle->SetThumbOffColor(color_provider->GetContentLayerColor(
        ash::AshColorProvider::ContentLayerType::kSwitchKnobColorInactive));
    toggle->SetTrackOnColor(color_provider->GetContentLayerColor(
        ash::AshColorProvider::ContentLayerType::kSwitchTrackColorActive));
    toggle->SetTrackOffColor(color_provider->GetContentLayerColor(
        ash::AshColorProvider::ContentLayerType::kSwitchTrackColorInactive));
  }
}

BEGIN_METADATA(InputMenuView)
END_METADATA

}  // namespace arc::input_overlay
