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
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
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

constexpr int kDropDownIconSizeDp = 16;
constexpr int kArrowButtonSizeDp = 48;
constexpr int kAdvancedViewButtonWidthDp = 190;
constexpr int kAdvancedViewButtonHeightDp = 16;
constexpr int kSpacingBetweenSelectionTitleAndButtonDp = 4;

constexpr int kNonEmptyWidth = 1;
constexpr int kNonEmptyHeight = 1;

constexpr char kMonitoringWarningClassName[] = "MonitoringWarning";
constexpr int kSpacingBetweenMonitoringWarningIconAndLabelDp = 8;
constexpr int kMonitoringWarningIconSizeDp = 20;

constexpr char kRightPaneViewClassName[] = "RightPaneView";
constexpr char kRightPaneAdvancedViewClassName[] = "RightPaneAdvancedView";

views::Label* CreateLabel(const std::u16string& text, SkColor color) {
  auto* label = new views::Label(text);
  label->SetSubpixelRenderingEnabled(false);
  label->SetAutoColorReadabilityEnabled(false);
  label->SetFontList(views::Label::GetDefaultFontList().Derive(
      0, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::NORMAL));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetEnabledColor(color);
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
    if (event->type() == ui::ET_MOUSE_PRESSED) {
      view_->ProcessPressedEvent(event->AsLocatedEvent());
    }
  }
  void OnGestureEvent(ui::GestureEvent* event) override {
    if ((event->type() == ui::ET_GESTURE_TAP ||
         event->type() == ui::ET_GESTURE_TAP_DOWN)) {
      view_->ProcessPressedEvent(event->AsLocatedEvent());
    }
  }
  void OnKeyEvent(ui::KeyEvent* event) override { view_->OnKeyEvent(event); }

  LoginExpandedPublicAccountView* view_;
};

}  // namespace

// Button with text on the left side and an icon on the right side.
class SelectionButtonView : public LoginButton {
 public:
  SelectionButtonView(PressedCallback callback, const std::u16string& text)
      : LoginButton(std::move(callback)) {
    SetAccessibleName(text);
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

    label_ = CreateLabel(
        text, AshColorProvider::Get()->GetContentLayerColor(
                  AshColorProvider::ContentLayerType::kTextColorPrimary));
    left_margin_view_ = add_horizontal_margin(left_margin_, label_container);
    label_container->AddChildView(label_);

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
    icon_->SetPreferredSize(
        gfx::Size(kDropDownIconSizeDp, kDropDownIconSizeDp));

    icon_container->AddChildView(icon_);
    right_margin_view_ = add_horizontal_margin(right_margin_, icon_container);
  }

  SelectionButtonView(const SelectionButtonView&) = delete;
  SelectionButtonView& operator=(const SelectionButtonView&) = delete;

  ~SelectionButtonView() override = default;

  // Return the preferred height of this view. This overrides the default
  // behavior in FillLayout::GetPreferredHeightForWidth which calculates the
  // height based on its child height.
  int GetHeightForWidth(int w) const override {
    return GetPreferredSize().height();
  }

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
    Layout();
  }

  void SetTextColor(SkColor color) { label_->SetEnabledColor(color); }
  void SetText(const std::u16string& text) {
    SetAccessibleName(text);
    label_->SetText(text);
    Layout();
  }

  void SetIcon(const gfx::VectorIcon& icon, SkColor color) {
    icon_->SetImage(gfx::CreateVectorIcon(icon, color));
  }

 private:
  int left_margin_ = 0;
  int right_margin_ = 0;
  views::Label* label_ = nullptr;
  views::ImageView* icon_ = nullptr;
  views::View* left_margin_view_ = nullptr;
  views::View* right_margin_view_ = nullptr;
};

// Container for the device monitoring warning. Composed of an optional warning
// icon on the left and a label to the right.
class MonitoringWarningView : public NonAccessibleView {
 public:
  MonitoringWarningView()
      : NonAccessibleView(kMonitoringWarningClassName),
        warning_type_(WarningType::kNone) {
    image_ = new views::ImageView();
    image_->SetImage(gfx::CreateVectorIcon(
        vector_icons::kWarningIcon, kMonitoringWarningIconSizeDp,
        AshColorProvider::Get()->GetContentLayerColor(
            AshColorProvider::ContentLayerType::kIconColorWarning)));
    image_->SetPreferredSize(
        gfx::Size(kMonitoringWarningIconSizeDp, kMonitoringWarningIconSizeDp));
    image_->SetVisible(false);
    AddChildView(image_);

    const std::u16string label_text = l10n_util::GetStringUTF16(
        IDS_ASH_LOGIN_PUBLIC_ACCOUNT_MONITORING_WARNING);
    label_ = CreateLabel(
        label_text, AshColorProvider::Get()->GetContentLayerColor(
                        AshColorProvider::ContentLayerType::kTextColorPrimary));
    label_->SetMultiLine(true);
    label_->SetLineHeight(kTextLineHeightDp);
    AddChildView(label_);
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

  // TODO(crbug/682266): MonitoringWarningview is effectively laid out as
  // BoxLayout with kSpacingBetweenMonitoringWarningIconAndLabelDp spacing
  // between its two child views. However, horizontal BoxLayout and FlexLayout
  // do not handle views that override GetHeightForWidth well, so it's
  // implemented ad-hoc here.
  int GetHeightForWidth(int w) const override {
    return image_->GetPreferredSize().height() +
           kSpacingBetweenMonitoringWarningIconAndLabelDp +
           label_->GetHeightForWidth(w);
  }

  void Layout() override {
    int y = 0;

    image_->SizeToPreferredSize();
    image_->SetPosition(gfx::Point{0, y});
    y = image_->bounds().bottom();

    y += kSpacingBetweenMonitoringWarningIconAndLabelDp;

    int label_height = label_->GetHeightForWidth(size().width());
    label_->SetSize(gfx::Size{size().width(), label_height});
    label_->SetPosition(gfx::Point{0, y});
    y = label_->bounds().bottom();
  }

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
    } else {
      label_text = l10n_util::GetStringFUTF16(
          IDS_ASH_LOGIN_MANAGED_SESSION_MONITORING_SOFT_WARNING,
          base::UTF8ToUTF16(device_manager_.value()));
      image_->SetVisible(false);
    }
    label_->SetText(label_text);
    InvalidateLayout();
    Layout();
  }

  friend class LoginExpandedPublicAccountView::TestApi;

  WarningType warning_type_;
  absl::optional<std::string> device_manager_;
  views::ImageView* image_;
  views::Label* label_;
};

// Implements the right part of the expanded public session view.
class RightPaneView : public NonAccessibleView {
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

    views::StyledLabel::RangeStyleInfo style;
    style.custom_font = learn_more_label_->GetFontList().Derive(
        0, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::NORMAL);
    style.override_color = AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary);
    learn_more_label_->AddStyleRange(gfx::Range(0, offset), style);

    views::StyledLabel::RangeStyleInfo link_style =
        views::StyledLabel::RangeStyleInfo::CreateForLink(on_learn_more_tapped);
    const SkColor blue = AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kButtonLabelColorBlue);
    link_style.override_color = blue;
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
    advanced_view_button_->SetTextColor(blue);
    advanced_view_button_->SetIcon(kLoginScreenButtonDropdownIcon, blue);
    advanced_view_button_->SetPreferredSize(
        gfx::Size(kAdvancedViewButtonWidthDp, kAdvancedViewButtonHeightDp));
    AddChildView(advanced_view_button_);

    advanced_view_button_->SetProperty(
        views::kMarginsKey, gfx::Insets().set_bottom(
                                views::LayoutProvider::Get()->GetDistanceMetric(
                                    views::DISTANCE_RELATED_CONTROL_VERTICAL)));

    // Create advanced view.
    advanced_view_ = new NonAccessibleView(kRightPaneAdvancedViewClassName);
    advanced_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
    advanced_view_->SetVisible(false);
    AddChildView(advanced_view_);

    const SkColor selection_menu_title_color =
        AshColorProvider::Get()->GetContentLayerColor(
            AshColorProvider::ContentLayerType::kTextColorSecondary);

    language_title_ = CreateLabel(
        l10n_util::GetStringUTF16(IDS_ASH_LOGIN_LANGUAGE_SELECTION_SELECT),
        selection_menu_title_color);
    advanced_view_->AddChildView(language_title_);
    language_title_->SetProperty(
        views::kMarginsKey,
        gfx::Insets().set_bottom(kSpacingBetweenSelectionTitleAndButtonDp));

    keyboard_title_ = CreateLabel(
        l10n_util::GetStringUTF16(IDS_ASH_LOGIN_KEYBOARD_SELECTION_SELECT),
        selection_menu_title_color);
    advanced_view_->AddChildView(keyboard_title_);
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

  void UpdateForUser(const LoginUserInfo& user) {
    DCHECK_EQ(user.basic_user_info.type,
              user_manager::USER_TYPE_PUBLIC_ACCOUNT);
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
    int selected_language_index = 0;
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
      advanced_view_->RemoveChildViewT(language_menu_view_);
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
    int selected_keyboard_index = 0;
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
      advanced_view_->RemoveChildViewT(keyboard_menu_view_);
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
    // TODO(crbug.com/984021) change to LaunchSamlPublicSession which would
    // take |selected_language_item_value_| and |selected_keyboard_item_value_|
    // too.
    if (current_user_.public_account_info->using_saml) {
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

  SelectionButtonView* advanced_view_button_ = nullptr;
  views::View* advanced_view_ = nullptr;
  views::View* language_title_ = nullptr;
  views::View* keyboard_title_ = nullptr;
  views::StyledLabel* learn_more_label_ = nullptr;

  PublicAccountMenuView* language_menu_view_ = nullptr;
  PublicAccountMenuView* keyboard_menu_view_ = nullptr;

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
  SetPreferredSize(GetPreferredSizeLandscape());
  layout_ = SetLayoutManager(std::make_unique<views::BoxLayout>());

  user_view_ = new LoginUserView(
      LoginDisplayStyle::kExtraSmall, false /*show_dropdown*/,
      base::DoNothing(), base::RepeatingClosure(), base::RepeatingClosure());
  user_view_->SetForceOpaque(true);
  user_view_->SetTapEnabled(false);

  left_pane_ = new NonAccessibleView();
  AddChildView(left_pane_);
  left_pane_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  left_pane_->AddChildView(user_view_);

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
  separator_->SetBackground(views::CreateSolidBackground(
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kSeparatorColor)));

  right_pane_ = new RightPaneView(
      base::BindRepeating(&LoginExpandedPublicAccountView::ShowWarningDialog,
                          base::Unretained(this)));
  AddChildView(right_pane_);

  submit_button_ = AddChildView(std::make_unique<ArrowButtonView>(
      base::BindRepeating(&RightPaneView::Login, base::Unretained(right_pane_)),
      kArrowButtonSizeDp));
  submit_button_->SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_ASH_LOGIN_PUBLIC_ACCOUNT_LOG_IN_BUTTON_ACCESSIBLE_NAME));
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

int LoginExpandedPublicAccountView::GetHeightForWidth(int width) const {
  if (width >= GetPreferredSizeLandscape().width()) {
    return GetPreferredSizeLandscape().height();
  }
  return GetPreferredSizePortrait().height();
}

void LoginExpandedPublicAccountView::Layout() {
  View::Layout();

  submit_button_->SizeToPreferredSize();
  const int submit_button_x =
      size().width() - kPaddingDp - submit_button_->size().width();
  const int submit_button_y =
      size().height() - kPaddingDp - submit_button_->size().height();
  submit_button_->SetPosition(gfx::Point{submit_button_x, submit_button_y});
}

void LoginExpandedPublicAccountView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(GetColorProvider()->GetColor(kColorAshShieldAndBase80));
  flags.setAntiAlias(true);
  canvas->DrawRoundRect(GetContentsBounds(), kRoundRectCornerRadiusDp, flags);
}

void LoginExpandedPublicAccountView::OnKeyEvent(ui::KeyEvent* event) {
  if (!GetVisible() || event->type() != ui::ET_KEY_PRESSED) {
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

  left_pane_->SetPreferredSize(absl::nullopt);
  left_pane_->SetProperty(views::kMarginsKey,
                          gfx::Insets::TLBR(kPaddingDp, kPaddingDp,
                                            kPortraitPaneSpacing, kPaddingDp));

  separator_->SetVisible(false);

  right_pane_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(0, kPaddingDp, kPaddingDp, kPaddingDp));
}

}  // namespace ash
