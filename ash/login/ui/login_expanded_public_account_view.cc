// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_expanded_public_account_view.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "ash/login/login_screen_controller.h"
#include "ash/login/ui/arrow_button_view.h"
#include "ash/login/ui/login_button.h"
#include "ash/login/ui/login_user_view.h"
#include "ash/login/ui/public_account_monitoring_info_dialog.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/login_types.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/system_shadow.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/mouse_constants.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

constexpr const char kLoginExpandedPublicAccountViewClassName[] =
    "LoginExpandedPublicAccountView";

constexpr int kPaddingDp = 24;

// TODO(b/1312900): Width increases by 40dp if we add the gutter.
constexpr int kLandscapeWidthDp = 723;
constexpr int kLandscapeHeightDp = 269;

constexpr int kPortraitWidthDp = 458;
constexpr int kPortraitHeightDp = 485;

constexpr int kLandscapeLeftPaneWidthDp = 253;
constexpr int kSeparatorThicknessDp = 1;
constexpr int kSeparatorMarginDp = 20;

constexpr int kPortraitPaneSpacing = 24;

constexpr int kTextLineHeightDp = 16;
constexpr int kRoundRectCornerRadiusDp = 2;
constexpr int kJellyRoundRectCornerRadiusDp = 8;

constexpr int kDropDownIconSizeDp = 16;
constexpr int kJellyDropDownIconSizeDp = 20;
constexpr int kArrowButtonSizeDp = 48;
constexpr int kAdvancedViewButtonWidthDp = 190;
constexpr int kAdvancedViewButtonHeightDp = 16;
constexpr int kJellyAdvancedViewButtonHeightDp = 20;
constexpr int kSpacingBetweenSelectionTitleAndButtonDp = 4;

constexpr int kNonEmptyWidth = 1;
constexpr int kNonEmptyHeight = 1;

constexpr char kMonitoringWarningClassName[] = "MonitoringWarning";
constexpr int kSpacingBetweenMonitoringWarningIconAndLabelDp = 8;
constexpr int kMonitoringWarningIconSizeDp = 20;

constexpr char kRightPaneViewClassName[] = "RightPaneView";
constexpr char kRightPaneAdvancedViewClassName[] = "RightPaneAdvancedView";

views::Label* CreateLabel(const std::u16string& text, ui::ColorId color_id) {
  auto* label = new views::Label(text);
  label->SetSubpixelRenderingEnabled(false);
  label->SetAutoColorReadabilityEnabled(false);
  label->SetFontList(views::Label::GetDefaultFontList().Derive(
      0, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::NORMAL));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetEnabledColorId(color_id);
  return label;
}

class LoginExpandedPublicAccountEventHandler : public ui::EventHandler {
 public:
  explicit LoginExpandedPublicAccountEventHandler(
      LoginExpandedPublicAccountView* view)
      : view_(view) {
    Shell::Get()->AddPreTargetHandler(this);
  }

  LoginExpandedPublicAccountEventHandler(
      const LoginExpandedPublicAccountEventHandler&) = delete;
  LoginExpandedPublicAccountEventHandler& operator=(
      const LoginExpandedPublicAccountEventHandler&) = delete;

  ~LoginExpandedPublicAccountEventHandler() override {
    Shell::Get()->RemovePreTargetHandler(this);
  }

 private:
  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override {
    if (event->type() == ui::EventType::kMousePressed) {
      view_->ProcessPressedEvent(event->AsLocatedEvent());
    }
  }
  void OnGestureEvent(ui::GestureEvent* event) override {
    if ((event->type() == ui::EventType::kGestureTap ||
         event->type() == ui::EventType::kGestureTapDown)) {
      view_->ProcessPressedEvent(event->AsLocatedEvent());
    }
  }
  void OnKeyEvent(ui::KeyEvent* event) override { view_->OnKeyEvent(event); }

  raw_ptr<LoginExpandedPublicAccountView> view_;
};

}  // namespace

// Button with text on the left side and an icon on the right side.
class SelectionButtonView : public LoginButton {
  METADATA_HEADER(SelectionButtonView, LoginButton)

 public:
  SelectionButtonView(PressedCallback callback, const std::u16string& text)
      : LoginButton(std::move(callback)) {
    GetViewAccessibility().SetName(text);
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    SetFocusBehavior(FocusBehavior::ALWAYS);
    SetLayoutManager(std::make_unique<views::FillLayout>());
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::OFF);

    auto add_horizontal_margin = [&](int width,
                                     views::View* parent) -> views::View* {
      auto* margin = new NonAccessibleView();
      margin->SetPreferredSize(gfx::Size(width, kNonEmptyHeight));
      parent->AddChildView(margin);
      return margin;
    };

    auto* label_container = new NonAccessibleView();
    label_container->SetCanProcessEventsWithinSubtree(false);
    views::BoxLayout* label_layout =
        label_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal));
    label_layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kStart);
    AddChildView(label_container);

    const bool is_jelly = chromeos::features::IsJellyrollEnabled();
    label_ = CreateLabel(text, is_jelly ? static_cast<ui::ColorId>(
                                              cros_tokens::kCrosSysOnSurface)
                                        : kColorAshTextColorPrimary);
    left_margin_view_ = add_horizontal_margin(left_margin_, label_container);
    label_container->AddChildView(label_.get());

    auto* icon_container = new NonAccessibleView();
    icon_container->SetCanProcessEventsWithinSubtree(false);
    views::BoxLayout* icon_layout =
        icon_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal));
    icon_layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kEnd);
    AddChildView(icon_container);

    icon_ = new views::ImageView;
    icon_->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
    int icon_size = is_jelly ? kJellyDropDownIconSizeDp : kDropDownIconSizeDp;
    icon_->SetPreferredSize(gfx::Size(icon_size, icon_size));

    icon_container->AddChildView(icon_.get());
    right_margin_view_ = add_horizontal_margin(right_margin_, icon_container);
  }

  SelectionButtonView(const SelectionButtonView&) = delete;
  SelectionButtonView& operator=(const SelectionButtonView&) = delete;

  ~SelectionButtonView() override = default;

  ui::Cursor GetCursor(const ui::MouseEvent& event) override {
    return ui::mojom::CursorType::kHand;
  }

  void SetMargins(int left, int right) {
    if (left_margin_ == left && right_margin_ == right) {
      return;
    }

    left_margin_ = left;
    right_margin_ = right;
    left_margin_view_->SetPreferredSize(
        gfx::Size(left_margin_, kNonEmptyHeight));
    right_margin_view_->SetPreferredSize(
        gfx::Size(right_margin_, kNonEmptyHeight));
    DeprecatedLayoutImmediately();
  }

  void SetTextColorId(ui::ColorId color_id) {
    label_->SetEnabledColorId(color_id);
  }
  void SetText(const std::u16string& text) {
    GetViewAccessibility().SetName(text);
    label_->SetText(text);
    DeprecatedLayoutImmediately();
  }

  void SetIcon(const gfx::VectorIcon& icon, ui::ColorId color_id) {
    icon_->SetImage(ui::ImageModel::FromVectorIcon(icon, color_id));
  }

 private:
  int left_margin_ = 0;
  int right_margin_ = 0;
  raw_ptr<views::Label> label_ = nullptr;
  raw_ptr<views::ImageView> icon_ = nullptr;
  raw_ptr<views::View> left_margin_view_ = nullptr;
  raw_ptr<views::View> right_margin_view_ = nullptr;
};

BEGIN_METADATA(SelectionButtonView)
END_METADATA

// Container for the device monitoring warning. Composed of an optional warning
// icon on the left and a label to the right.
class MonitoringWarningView : public NonAccessibleView {
  METADATA_HEADER(MonitoringWarningView, NonAccessibleView)

 public:
  MonitoringWarningView()
      : NonAccessibleView(kMonitoringWarningClassName),
        warning_type_(WarningType::kNone) {
    const bool is_jelly = chromeos::features::IsJellyrollEnabled();

    const std::u16string label_text = l10n_util::GetStringUTF16(
        IDS_ASH_LOGIN_PUBLIC_ACCOUNT_MONITORING_WARNING);
    views::Builder<views::View>(this)
        .SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::LayoutOrientation::kVertical, gfx::Insets(),
            kSpacingBetweenMonitoringWarningIconAndLabelDp))
        .AddChildren(
            views::Builder<views::ImageView>()
                .CopyAddressTo(&image_)
                .SetVisible(false)
                .SetImage(ui::ImageModel::FromVectorIcon(
                    vector_icons::kWarningIcon,
                    is_jelly ? static_cast<ui::ColorId>(
                                   cros_tokens::kCrosSysOnSurface)
                             : kColorAshIconColorWarning,
                    kMonitoringWarningIconSizeDp)),
            views::Builder<views::View>()
                .CopyAddressTo(&placeholder_)
                .SetPreferredSize(gfx::Size(0, kMonitoringWarningIconSizeDp)),
            views::Builder<views::Label>(
                base::WrapUnique(CreateLabel(
                    label_text, is_jelly ? static_cast<ui::ColorId>(
                                               cros_tokens::kCrosSysOnSurface)
                                         : kColorAshTextColorPrimary)))
                .SetMultiLine(true)
                .CopyAddressTo(&label_)
                .SetLineHeight(kTextLineHeightDp))
        .BuildChildren();
  }

  enum class WarningType { kNone, kSoftWarning, kFullWarning };

  void UpdateForUser(const LoginUserInfo& user) {
    device_manager_ = user.public_account_info->device_enterprise_manager;
    UpdateLabel();
  }

  void SetWarningType(WarningType warning_type) {
    warning_type_ = warning_type;
    UpdateLabel();
  }

  MonitoringWarningView(const MonitoringWarningView&) = delete;
  MonitoringWarningView& operator=(const MonitoringWarningView&) = delete;

  ~MonitoringWarningView() override = default;

 private:
  void UpdateLabel() {
    // Call sequence of UpdateForUser() and SetWarningType() is not clear.
    // In case SetWarningType is called first there is a need to wait for
    // device_manager_ is set.
    if (warning_type_ == WarningType::kNone || !device_manager_.has_value()) {
      return;
    }
    std::u16string label_text;
    if (warning_type_ == WarningType::kFullWarning) {
      label_text = l10n_util::GetStringFUTF16(
          IDS_ASH_LOGIN_MANAGED_SESSION_MONITORING_FULL_WARNING,
          base::UTF8ToUTF16(device_manager_.value()));
      image_->SetVisible(true);
      placeholder_->SetVisible(false);
    } else {
      label_text = l10n_util::GetStringFUTF16(
          IDS_ASH_LOGIN_MANAGED_SESSION_MONITORING_SOFT_WARNING,
          base::UTF8ToUTF16(device_manager_.value()));
      image_->SetVisible(false);
      placeholder_->SetVisible(true);
    }
    label_->SetText(label_text);
    InvalidateLayout();
    DeprecatedLayoutImmediately();
  }

  friend class LoginExpandedPublicAccountView::TestApi;

  WarningType warning_type_;
  std::optional<std::string> device_manager_;
  raw_ptr<views::ImageView> image_;
  raw_ptr<views::View> placeholder_;
  raw_ptr<views::Label> label_;
};

BEGIN_METADATA(MonitoringWarningView)
END_METADATA

// Implements the right part of the expanded public session view.
class RightPaneView : public NonAccessibleView {
  METADATA_HEADER(RightPaneView, NonAccessibleView)

 public:
  explicit RightPaneView(const base::RepeatingClosure& on_learn_more_tapped)
      : NonAccessibleView(kRightPaneViewClassName) {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kStart);

    // Create "learn more" label.
    const std::u16string link = l10n_util::GetStringUTF16(IDS_ASH_LEARN_MORE);
    size_t offset;
    const std::u16string text = l10n_util::GetStringFUTF16(
        IDS_ASH_LOGIN_PUBLIC_ACCOUNT_SIGNOUT_REMINDER, link, &offset);
    learn_more_label_ = AddChildView(std::make_unique<views::StyledLabel>());
    learn_more_label_->SetText(text);

    const bool is_jelly = chromeos::features::IsJellyrollEnabled();

    views::StyledLabel::RangeStyleInfo style;
    style.custom_font = learn_more_label_->GetFontList().Derive(
        0, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::NORMAL);
    style.override_color_id =
        is_jelly ? static_cast<ui::ColorId>(cros_tokens::kCrosSysOnSurface)
                 : kColorAshTextColorPrimary;
    learn_more_label_->AddStyleRange(gfx::Range(0, offset), style);

    views::StyledLabel::RangeStyleInfo link_style =
        views::StyledLabel::RangeStyleInfo::CreateForLink(on_learn_more_tapped);
    link_style.override_color_id =
        is_jelly ? static_cast<ui::ColorId>(cros_tokens::kCrosSysPrimary)
                 : kColorAshButtonLabelColorBlue;
    learn_more_label_->AddStyleRange(gfx::Range(offset, offset + link.length()),
                                     link_style);
    learn_more_label_->SetAutoColorReadabilityEnabled(false);

    learn_more_label_->SetProperty(
        views::kMarginsKey,
        gfx::Insets().set_bottom(
            views::LayoutProvider::Get()->GetDistanceMetric(
                views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));

    // Create button to show/hide advanced view.
    advanced_view_button_ = new SelectionButtonView(
        base::BindRepeating(&RightPaneView::AdvancedViewButtonPressed,
                            base::Unretained(this)),
        l10n_util::GetStringUTF16(
            IDS_ASH_LOGIN_PUBLIC_SESSION_LANGUAGE_AND_INPUT));
    ui::ColorId advanced_view_button_color_id =
        is_jelly ? static_cast<ui::ColorId>(cros_tokens::kCrosSysPrimary)
                 : kColorAshButtonLabelColorBlue;
    int advanced_view_button_icon_size = is_jelly
                                             ? kJellyAdvancedViewButtonHeightDp
                                             : kAdvancedViewButtonHeightDp;
    advanced_view_button_->SetTextColorId(advanced_view_button_color_id);
    advanced_view_button_->SetIcon(kLoginScreenButtonDropdownIcon,
                                   advanced_view_button_color_id);

    advanced_view_button_->SetPreferredSize(
        gfx::Size(kAdvancedViewButtonWidthDp, advanced_view_button_icon_size));
    AddChildView(advanced_view_button_.get());

    advanced_view_button_->SetProperty(
        views::kMarginsKey, gfx::Insets().set_bottom(
                                views::LayoutProvider::Get()->GetDistanceMetric(
                                    views::DISTANCE_RELATED_CONTROL_VERTICAL)));

    // Create advanced view.
    advanced_view_ = new NonAccessibleView(kRightPaneAdvancedViewClassName);
    advanced_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
    advanced_view_->SetVisible(false);
    AddChildView(advanced_view_.get());

    language_title_ = CreateLabel(
        l10n_util::GetStringUTF16(IDS_ASH_LOGIN_LANGUAGE_SELECTION_SELECT),
        kColorAshTextColorSecondary);
    advanced_view_->AddChildView(language_title_.get());
    language_title_->SetProperty(
        views::kMarginsKey,
        gfx::Insets().set_bottom(kSpacingBetweenSelectionTitleAndButtonDp));

    keyboard_title_ = CreateLabel(
        l10n_util::GetStringUTF16(IDS_ASH_LOGIN_KEYBOARD_SELECTION_SELECT),
        kColorAshTextColorSecondary);
    advanced_view_->AddChildView(keyboard_title_.get());
    keyboard_title_->SetProperty(
        views::kMarginsKey,
        gfx::Insets()
            .set_top(views::LayoutProvider::Get()->GetDistanceMetric(
                views::DISTANCE_UNRELATED_CONTROL_VERTICAL))
            .set_bottom(kSpacingBetweenSelectionTitleAndButtonDp));
  }

  RightPaneView(const RightPaneView&) = delete;
  RightPaneView& operator=(const RightPaneView&) = delete;

  ~RightPaneView() override = default;

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return GetLayoutManager()->GetPreferredSize(this, available_size);
  }

  void UpdateForUser(const LoginUserInfo& user) {
    DCHECK_EQ(user.basic_user_info.type,
              user_manager::UserType::kPublicAccount);
    current_user_ = user;
    if (!language_changed_by_user_) {
      selected_language_item_value_ = user.public_account_info->default_locale;
    }

    PopulateLanguageItems(user.public_account_info->available_locales);

    if (user.public_account_info->default_locale ==
        selected_language_item_value_) {
      PopulateKeyboardItems(user.public_account_info->keyboard_layouts);
    }

    if (!show_advanced_changed_by_user_) {
      advanced_view_->SetVisible(user.public_account_info->show_advanced_view);
    }
  }

  void OnLanguageSelected(const std::string& value) {
    language_changed_by_user_ = true;
    selected_language_item_value_ = value;
    current_user_.public_account_info->default_locale = value;

    // User changed the preferred locale, request to get corresponding keyboard
    // layouts.
    Shell::Get()
        ->login_screen_controller()
        ->RequestPublicSessionKeyboardLayouts(
            current_user_.basic_user_info.account_id, value);
  }

  void OnKeyboardSelected(const std::string& value) {
    selected_keyboard_item_value_ = value;
  }

  void PopulateLanguageItems(const std::vector<LocaleItem>& locales) {
    language_items_.clear();
    std::optional<int> selected_language_index = std::nullopt;
    for (const auto& locale : locales) {
      PublicAccountMenuView::Item item;
      if (locale.group_name) {
        item.title = locale.group_name.value();
        item.is_group = true;
      } else {
        item.title = locale.title;
        item.value = locale.language_code;
        item.is_group = false;
        item.selected = selected_language_item_value_ == locale.language_code;
      }
      if (selected_language_item_value_ == locale.language_code) {
        selected_language_index = language_items_.size();
      }
      language_items_.push_back(item);
    }

    if (language_menu_view_) {
      advanced_view_->RemoveChildViewT(language_menu_view_.get());
      language_menu_view_ = nullptr;
    }
    auto language_menu_view = std::make_unique<PublicAccountMenuView>(
        language_items_, selected_language_index,
        base::BindRepeating(&RightPaneView::OnLanguageSelected,
                            weak_factory_.GetWeakPtr()));
    language_menu_view->SetTooltipTextAndAccessibleName(
        l10n_util::GetStringUTF16(
            IDS_ASH_LOGIN_PUBLIC_ACCOUNT_LANGUAGE_MENU_ACCESSIBLE_NAME));

    const size_t index =
        advanced_view_->GetIndexOf(language_title_).value() + 1;
    language_menu_view_ =
        advanced_view_->AddChildViewAt(std::move(language_menu_view), index);
  }

  void PopulateKeyboardItems(
      const std::vector<InputMethodItem>& keyboard_layouts) {
    keyboard_items_.clear();
    std::optional<int> selected_keyboard_index = std::nullopt;
    for (const auto& keyboard : keyboard_layouts) {
      PublicAccountMenuView::Item item;
      item.title = keyboard.title;
      item.value = keyboard.ime_id;
      item.is_group = false;
      item.selected = keyboard.selected;
      if (keyboard.selected) {
        selected_keyboard_index = keyboard_items_.size();
        selected_keyboard_item_value_ = item.value;
      }
      keyboard_items_.push_back(item);
    }

    if (keyboard_menu_view_) {
      advanced_view_->RemoveChildViewT(keyboard_menu_view_.get());
      keyboard_menu_view_ = nullptr;
    }
    auto keyboard_menu_view = std::make_unique<PublicAccountMenuView>(
        keyboard_items_, selected_keyboard_index,
        base::BindRepeating(&RightPaneView::OnKeyboardSelected,
                            weak_factory_.GetWeakPtr()));
    keyboard_menu_view->SetTooltipTextAndAccessibleName(
        l10n_util::GetStringUTF16(
            IDS_ASH_LOGIN_PUBLIC_ACCOUNT_KEYBOARD_MENU_ACCESSIBLE_NAME));

    const size_t index =
        advanced_view_->GetIndexOf(keyboard_title_).value() + 1;
    keyboard_menu_view_ =
        advanced_view_->AddChildViewAt(std::move(keyboard_menu_view), index);
  }

  PublicAccountMenuView* GetLanguageMenuView() { return language_menu_view_; }

  PublicAccountMenuView* GetKeyboardMenuView() { return keyboard_menu_view_; }

  // Reset local states.
  void Reset() {
    show_advanced_changed_by_user_ = false;
    language_changed_by_user_ = false;
  }

  void Login() {
    // TODO(crbug.com/40636049) change to LaunchSamlPublicSession which would
    // take |selected_language_item_value_| and |selected_keyboard_item_value_|
    // too.
    if (current_user_.public_account_info->using_saml) {
      // TODO(b/333882432): Remove this log after the bug fixed.
      LOG(WARNING) << "b/333882432: RightPaneView::Login";
      Shell::Get()->login_screen_controller()->ShowGaiaSignin(
          current_user_.basic_user_info.account_id);
    } else {
      Shell::Get()->login_screen_controller()->LaunchPublicSession(
          current_user_.basic_user_info.account_id,
          selected_language_item_value_, selected_keyboard_item_value_);
    }
  }

 private:
  friend class LoginExpandedPublicAccountView::TestApi;

  void AdvancedViewButtonPressed() {
    show_advanced_changed_by_user_ = true;
    advanced_view_->SetVisible(!advanced_view_->GetVisible());
  }

  LoginUserInfo current_user_;

  raw_ptr<SelectionButtonView> advanced_view_button_ = nullptr;
  raw_ptr<views::View> advanced_view_ = nullptr;
  raw_ptr<views::View> language_title_ = nullptr;
  raw_ptr<views::View> keyboard_title_ = nullptr;
  raw_ptr<views::StyledLabel> learn_more_label_ = nullptr;

  raw_ptr<PublicAccountMenuView, DanglingUntriaged> language_menu_view_ =
      nullptr;
  raw_ptr<PublicAccountMenuView, DanglingUntriaged> keyboard_menu_view_ =
      nullptr;

  std::string selected_language_item_value_;
  std::string selected_keyboard_item_value_;
  std::vector<PublicAccountMenuView::Item> language_items_;
  std::vector<PublicAccountMenuView::Item> keyboard_items_;

  // Local states to check if the locale and whether to show advanced view
  // has been changed by the user. This ensures user action won't be overridden
  // by the default settings and it will be reset after this view is hidden.
  // Keyboard selection is not tracked here because it's depending on which
  // locale user selects, so the previously selected keyboard might not be
  // applicable for the current locale.
  bool show_advanced_changed_by_user_ = false;
  bool language_changed_by_user_ = false;

  base::WeakPtrFactory<RightPaneView> weak_factory_{this};
};

BEGIN_METADATA(RightPaneView)
END_METADATA

LoginExpandedPublicAccountView::TestApi::TestApi(
    LoginExpandedPublicAccountView* view)
    : view_(view) {}

LoginExpandedPublicAccountView::TestApi::~TestApi() = default;

LoginUserView* LoginExpandedPublicAccountView::TestApi::user_view() {
  return view_->user_view_;
}

views::View* LoginExpandedPublicAccountView::TestApi::advanced_view_button() {
  return view_->right_pane_->advanced_view_button_;
}

ArrowButtonView* LoginExpandedPublicAccountView::TestApi::submit_button() {
  return view_->submit_button_;
}

views::View* LoginExpandedPublicAccountView::TestApi::advanced_view() {
  return view_->right_pane_->advanced_view_;
}

PublicAccountMonitoringInfoDialog*
LoginExpandedPublicAccountView::TestApi::learn_more_dialog() {
  return view_->learn_more_dialog_;
}

views::StyledLabel*
LoginExpandedPublicAccountView::TestApi::learn_more_label() {
  return view_->right_pane_->learn_more_label_;
}

PublicAccountMenuView*
LoginExpandedPublicAccountView::TestApi::language_menu_view() {
  return view_->right_pane_->language_menu_view_;
}

PublicAccountMenuView*
LoginExpandedPublicAccountView::TestApi::keyboard_menu_view() {
  return view_->right_pane_->keyboard_menu_view_;
}

std::string
LoginExpandedPublicAccountView::TestApi::selected_language_item_value() {
  return view_->right_pane_->selected_language_item_value_;
}

std::string
LoginExpandedPublicAccountView::TestApi::selected_keyboard_item_value() {
  return view_->right_pane_->selected_keyboard_item_value_;
}

views::ImageView*
LoginExpandedPublicAccountView::TestApi::monitoring_warning_icon() {
  if (view_->monitoring_warning_view_) {
    return view_->monitoring_warning_view_->image_;
  }
  return nullptr;
}

views::Label*
LoginExpandedPublicAccountView::TestApi::monitoring_warning_label() {
  if (view_->monitoring_warning_view_) {
    return view_->monitoring_warning_view_->label_;
  }
  return nullptr;
}

void LoginExpandedPublicAccountView::TestApi::ResetUserForTest() {
  if (view_->monitoring_warning_view_) {
    view_->monitoring_warning_view_->device_manager_.reset();
  }
}

bool LoginExpandedPublicAccountView::TestApi::SelectLanguage(
    const std::string& language_code) {
  for (PublicAccountMenuView::Item item : view_->right_pane_->language_items_) {
    if (item.value == language_code) {
      view_->right_pane_->OnLanguageSelected(item.value);
      return true;
    }
  }
  return false;
}

bool LoginExpandedPublicAccountView::TestApi::SelectKeyboard(
    const std::string& ime_id) {
  for (PublicAccountMenuView::Item item : view_->right_pane_->keyboard_items_) {
    if (item.value == ime_id) {
      view_->right_pane_->OnKeyboardSelected(item.value);
      return true;
    }
  }
  return false;
}

std::vector<LocaleItem> LoginExpandedPublicAccountView::TestApi::GetLocales() {
  std::vector<LocaleItem> locales;
  for (PublicAccountMenuView::Item item : view_->right_pane_->language_items_) {
    LocaleItem locale;
    locale.title = item.title;
    locale.language_code = item.value;
    locales.push_back(locale);
  }
  return locales;
}

LoginExpandedPublicAccountView::LoginExpandedPublicAccountView(
    const OnPublicSessionViewDismissed& on_dismissed)
    : NonAccessibleView(kLoginExpandedPublicAccountViewClassName),
      on_dismissed_(on_dismissed),
      event_handler_(
          std::make_unique<LoginExpandedPublicAccountEventHandler>(this)) {
  if (chromeos::features::IsJellyrollEnabled()) {
    SetBackground(views::CreateThemedRoundedRectBackground(
        cros_tokens::kCrosSysSystemBaseElevated,
        kJellyRoundRectCornerRadiusDp));
    SetBorder(std::make_unique<views::HighlightBorder>(
        kJellyRoundRectCornerRadiusDp,
        views::HighlightBorder::Type::kHighlightBorderOnShadow));
    shadow_ = SystemShadow::CreateShadowOnNinePatchLayerForView(
        this, SystemShadow::Type::kElevation12);
    shadow_->SetRoundedCornerRadius(kJellyRoundRectCornerRadiusDp);
  } else {
    SetBackground(views::CreateThemedRoundedRectBackground(
        kColorAshShieldAndBase80, kRoundRectCornerRadiusDp));
  }

  SetPreferredSize(GetPreferredSizeLandscape());
  layout_ = SetLayoutManager(std::make_unique<views::BoxLayout>());

  user_view_ =
      new LoginUserView(LoginDisplayStyle::kExtraSmall, false /*show_dropdown*/,
                        base::DoNothing(), base::RepeatingClosure());
  user_view_->SetForceOpaque(true);
  user_view_->SetTapEnabled(false);

  left_pane_ = new NonAccessibleView();
  AddChildView(left_pane_.get());
  left_pane_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  left_pane_->AddChildView(user_view_.get());

  const bool enable_warning = Shell::Get()->local_state()->GetBoolean(
      prefs::kManagedGuestSessionPrivacyWarningsEnabled);
  if (enable_warning) {
    views::View* padding =
        left_pane_->AddChildView(std::make_unique<NonAccessibleView>());
    padding->SetPreferredSize(gfx::Size{kNonEmptyWidth, 24});
    monitoring_warning_view_ =
        left_pane_->AddChildView(std::make_unique<MonitoringWarningView>());
  }

  separator_ = AddChildView(std::make_unique<views::View>());
  ui::ColorId separator_color_id =
      chromeos::features::IsJellyrollEnabled()
          ? static_cast<ui::ColorId>(cros_tokens::kCrosSysSeparator)
          : kColorAshSeparatorColor;
  separator_->SetBackground(
      views::CreateThemedSolidBackground(separator_color_id));

  right_pane_ = new RightPaneView(
      base::BindRepeating(&LoginExpandedPublicAccountView::ShowWarningDialog,
                          base::Unretained(this)));
  AddChildView(right_pane_.get());

  submit_button_ = AddChildView(std::make_unique<ArrowButtonView>(
      base::BindRepeating(&RightPaneView::Login, base::Unretained(right_pane_)),
      kArrowButtonSizeDp));
  submit_button_->GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
      IDS_ASH_LOGIN_PUBLIC_ACCOUNT_LOG_IN_BUTTON_ACCESSIBLE_NAME));
  if (chromeos::features::IsJellyrollEnabled()) {
    submit_button_->SetBackgroundColorId(
        cros_tokens::kCrosSysSystemPrimaryContainer);
  }
  // `submit_button_` has absolute position and is laid out in our `Layout()`
  // override.
  submit_button_->SetProperty(views::kViewIgnoredByLayoutKey, true);
}

LoginExpandedPublicAccountView::~LoginExpandedPublicAccountView() = default;

// static
void LoginExpandedPublicAccountView::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(
      prefs::kManagedGuestSessionPrivacyWarningsEnabled, true);
}

void LoginExpandedPublicAccountView::ProcessPressedEvent(
    const ui::LocatedEvent* event) {
  if (!GetVisible()) {
    return;
  }

  // Keep this view to be visible until learn more dialog is dismissed.
  if (learn_more_dialog_ && learn_more_dialog_->IsVisible()) {
    return;
  }

  if (GetBoundsInScreen().Contains(event->root_location())) {
    return;
  }

  // Ignore press event if the language or keyboard menu is still running,
  // or if it had just been closed. Note that when we checked closed time,
  // if the menu has never been closed, closed time will be set to time 0 and
  // the time delta between now and then will be more than 100 ms.
  PublicAccountMenuView* language_menu_view =
      right_pane_->GetLanguageMenuView();
  if (language_menu_view &&
      (language_menu_view->IsMenuRunning() ||
       (base::TimeTicks::Now() - language_menu_view->GetClosedTime()) <
           views::kMinimumTimeBetweenButtonClicks)) {
    return;
  }

  PublicAccountMenuView* keyboard_menu_view =
      right_pane_->GetKeyboardMenuView();
  if (keyboard_menu_view &&
      (keyboard_menu_view->IsMenuRunning() ||
       (base::TimeTicks::Now() - language_menu_view->GetClosedTime()) <
           views::kMinimumTimeBetweenButtonClicks)) {
    return;
  }

  Hide();
}

void LoginExpandedPublicAccountView::UpdateForUser(const LoginUserInfo& user) {
  user_view_->UpdateForUser(user, false /*animate*/);
  right_pane_->UpdateForUser(user);
  if (monitoring_warning_view_) {
    monitoring_warning_view_->UpdateForUser(user);
  }
}

const LoginUserInfo& LoginExpandedPublicAccountView::current_user() const {
  return user_view_->current_user();
}

void LoginExpandedPublicAccountView::Hide() {
  shadow_.reset();
  SetVisible(false);
  right_pane_->Reset();
  on_dismissed_.Run();
}

void LoginExpandedPublicAccountView::ShowWarningDialog() {
  DCHECK(!learn_more_dialog_);
  learn_more_dialog_ =
      new PublicAccountMonitoringInfoDialog(weak_factory_.GetWeakPtr());
  learn_more_dialog_->Show();
}

void LoginExpandedPublicAccountView::OnLearnMoreDialogClosed() {
  learn_more_dialog_ = nullptr;
}

void LoginExpandedPublicAccountView::SetShowFullManagementDisclosure(
    bool show_full_management_disclosure) {
  if (monitoring_warning_view_) {
    monitoring_warning_view_->SetWarningType(
        show_full_management_disclosure
            ? MonitoringWarningView::WarningType::kFullWarning
            : MonitoringWarningView::WarningType::kSoftWarning);
  }
}

gfx::Size LoginExpandedPublicAccountView::GetPreferredSizeLandscape() {
  return gfx::Size{kLandscapeWidthDp, kLandscapeHeightDp};
}
gfx::Size LoginExpandedPublicAccountView::GetPreferredSizePortrait() {
  return gfx::Size{kPortraitWidthDp, kPortraitHeightDp};
}

void LoginExpandedPublicAccountView::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  if (bounds().width() >= bounds().height()) {
    UseLandscapeLayout();
  } else {
    UsePortraitLayout();
  }
}

void LoginExpandedPublicAccountView::Layout(PassKey) {
  LayoutSuperclass<View>(this);

  submit_button_->SizeToPreferredSize();
  const int submit_button_x =
      size().width() - kPaddingDp - submit_button_->size().width();
  const int submit_button_y =
      size().height() - kPaddingDp - submit_button_->size().height();
  submit_button_->SetPosition(gfx::Point{submit_button_x, submit_button_y});
}

void LoginExpandedPublicAccountView::OnKeyEvent(ui::KeyEvent* event) {
  if (!GetVisible() || event->type() != ui::EventType::kKeyPressed) {
    return;
  }

  // Give learn more dialog a chance to handle key event.
  if (learn_more_dialog_ && learn_more_dialog_->IsVisible()) {
    return;
  }

  if (event->key_code() == ui::KeyboardCode::VKEY_ESCAPE) {
    Hide();
  }
}

void LoginExpandedPublicAccountView::UseLandscapeLayout() {
  layout_->SetOrientation(views::BoxLayout::Orientation::kHorizontal);

  left_pane_->SetPreferredSize(
      gfx::Size{kLandscapeLeftPaneWidthDp, size().height() - 2 * kPaddingDp});
  left_pane_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(kPaddingDp, kPaddingDp, kPaddingDp, 0));

  separator_->SetVisible(true);
  separator_->SetPreferredSize(
      gfx::Size{kSeparatorThicknessDp, kNonEmptyHeight});
  separator_->SetProperty(views::kCrossAxisAlignmentKey,
                          views::LayoutAlignment::kStretch);
  separator_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(0, kSeparatorMarginDp, 0, kSeparatorMarginDp));

  right_pane_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(kPaddingDp, 0, kPaddingDp, kPaddingDp));
}

void LoginExpandedPublicAccountView::UsePortraitLayout() {
  layout_->SetOrientation(views::BoxLayout::Orientation::kVertical);

  left_pane_->SetPreferredSize(std::nullopt);
  left_pane_->SetProperty(views::kMarginsKey,
                          gfx::Insets::TLBR(kPaddingDp, kPaddingDp,
                                            kPortraitPaneSpacing, kPaddingDp));

  separator_->SetVisible(false);

  right_pane_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(0, kPaddingDp, kPaddingDp, kPaddingDp));
}

BEGIN_METADATA(LoginExpandedPublicAccountView)
END_METADATA

}  // namespace ash
