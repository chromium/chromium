// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_expanded_public_account_view.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/login/login_screen_controller.h"
#include "ash/login/ui/arrow_button_view.h"
#include "ash/login/ui/login_button.h"
#include "ash/login/ui/login_user_view.h"
#include "ash/login/ui/public_account_warning_dialog.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/login_types.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label_listener.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

constexpr const char kLoginExpandedPublicAccountViewClassName[] =
    "LoginExpandedPublicAccountView";
constexpr int kExpandedViewWidthDp = 600;
constexpr int kExpandedViewHeightDp = 324;

constexpr int kTextLineHeightDp = 16;
constexpr int kRoundRectCornerRadiusDp = 2;
constexpr int kBorderThicknessDp = 1;
constexpr int kRightPaneMarginDp = 28;
constexpr int kLabelMarginDp = 20;
constexpr int kLeftMarginForSelectionButton = 8;
constexpr int kRightMarginForSelectionButton = 3;

constexpr SkColor kPublicSessionBackgroundColor =
    SkColorSetARGB(0xAB, 0x00, 0x00, 0x00);
constexpr SkColor kBorderColor = SkColorSetA(SK_ColorWHITE, 0x33);
constexpr SkColor kPublicSessionBlueColor =
    SkColorSetARGB(0xDE, 0x7B, 0xAA, 0xF7);
constexpr SkColor kSelectionMenuTitleColor =
    SkColorSetARGB(0x57, 0xFF, 0xFF, 0xFF);
constexpr SkColor kArrowButtonColor = SkColorSetARGB(0xFF, 0x42, 0x85, 0xF4);

constexpr int kDropDownIconSizeDp = 16;
constexpr int kArrowButtonSizeDp = 40;
constexpr int kAdvancedViewButtonWidthDp = 190;
constexpr int kAdvancedViewButtonHeightDp = 16;
constexpr int kSelectionBoxWidthDp = 178;
constexpr int kSelectionBoxHeightDp = 28;
constexpr int kTopSpacingForLabelInAdvancedViewDp = 7;
constexpr int kTopSpacingForLabelInRegularViewDp = 65;
constexpr int kSpacingBetweenLabelsDp = 17;
constexpr int kSpacingBetweenSelectionMenusDp = 15;
constexpr int kSpacingBetweenSelectionTitleAndButtonDp = 3;
constexpr int kSpacingBetweenLanguageMenuAndAdvancedViewButtonDp = 4;
constexpr int kSpacingBetweenAdvancedViewButtonAndLabelDp = 32;
constexpr int kTopSpacingForUserViewDp = 62;

constexpr int kNonEmptyWidth = 1;
constexpr int kNonEmptyHeight = 1;

constexpr char kMonitoringWarningClassName[] = "MonitoringWarning";
constexpr int kSpacingBetweenMonitoringWarningIconAndLabelDp = 8;
constexpr int kMonitoringWarningIconSizeDp = 20;

views::Label* CreateLabel(const base::string16& text, SkColor color) {
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
  ~LoginExpandedPublicAccountEventHandler() override {
    Shell::Get()->RemovePreTargetHandler(this);
  }

 private:
  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override {
    if (event->type() == ui::ET_MOUSE_PRESSED)
      view_->ProcessPressedEvent(event->AsLocatedEvent());
  }
  void OnGestureEvent(ui::GestureEvent* event) override {
    if ((event->type() == ui::ET_GESTURE_TAP ||
         event->type() == ui::ET_GESTURE_TAP_DOWN)) {
      view_->ProcessPressedEvent(event->AsLocatedEvent());
    }
  }
  void OnKeyEvent(ui::KeyEvent* event) override { view_->OnKeyEvent(event); }

  LoginExpandedPublicAccountView* view_;

  DISALLOW_COPY_AND_ASSIGN(LoginExpandedPublicAccountEventHandler);
};

}  // namespace

// Button with text on the left side and an icon on the right side.
class SelectionButtonView : public LoginButton {
 public:
  SelectionButtonView(const base::string16& text,
                      views::ButtonListener* listener)
      : LoginButton(listener) {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    SetFocusBehavior(FocusBehavior::ALWAYS);
    SetLayoutManager(std::make_unique<views::FillLayout>());
    SetInkDropMode(InkDropMode::OFF);

    auto add_horizontal_margin = [&](int width,
                                     views::View* parent) -> views::View* {
      auto* margin = new NonAccessibleView();
      margin->SetPreferredSize(gfx::Size(width, kNonEmptyHeight));
      parent->AddChildView(margin);
      return margin;
    };

    auto* label_container = new NonAccessibleView();
    views::BoxLayout* label_layout =
        label_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal));
    label_layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kStart);
    AddChildView(label_container);

    label_ = CreateLabel(text, SK_ColorWHITE);
    left_margin_view_ = add_horizontal_margin(left_margin_, label_container);
    label_container->AddChildView(label_);

    auto* icon_container = new NonAccessibleView();
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

  ~SelectionButtonView() override = default;

  // Return the preferred height of this view. This overrides the default
  // behavior in FillLayout::GetPreferredHeightForWidth which calculates the
  // height based on its child height.
  int GetHeightForWidth(int w) const override {
    return GetPreferredSize().height();
  }

  void SetMargins(int left, int right) {
    if (left_margin_ == left && right_margin_ == right)
      return;

    left_margin_ = left;
    right_margin_ = right;
    left_margin_view_->SetPreferredSize(
        gfx::Size(left_margin_, kNonEmptyHeight));
    right_margin_view_->SetPreferredSize(
        gfx::Size(right_margin_, kNonEmptyHeight));
    Layout();
  }

  void SetTextColor(SkColor color) { label_->SetEnabledColor(color); }
  void SetText(const base::string16& text) {
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

  DISALLOW_COPY_AND_ASSIGN(SelectionButtonView);
};

// Container for the device monitoring warning.
class MonitoringWarningView : public NonAccessibleView {
 public:
  MonitoringWarningView()
      : NonAccessibleView(kMonitoringWarningClassName),
        warning_type_(WarningType::kNone) {
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
        kSpacingBetweenMonitoringWarningIconAndLabelDp));

    image_ = new views::ImageView();
    image_->SetImage(gfx::CreateVectorIcon(
        vector_icons::kWarningIcon, kMonitoringWarningIconSizeDp, SK_ColorRED));
    image_->SetPreferredSize(
        gfx::Size(kMonitoringWarningIconSizeDp, kMonitoringWarningIconSizeDp));
    image_->SetVisible(false);
    AddChildView(image_);

    const base::string16 label_text = l10n_util::GetStringUTF16(
        IDS_ASH_LOGIN_PUBLIC_ACCOUNT_MONITORING_WARNING);
    label_ = CreateLabel(label_text, SK_ColorWHITE);
    label_->SetMultiLine(true);
    label_->SetLineHeight(kTextLineHeightDp);
    AddChildView(label_);
  }

  enum class WarningType { kNone, kSoftWarning, kFullWarning };

  void UpdateForUser(const LoginUserInfo& user) {
    enterprise_domain_ = user.public_account_info->enterprise_domain;
    UpdateLabel();
  }

  void SetWarningType(WarningType warning_type) {
    warning_type_ = warning_type;
    UpdateLabel();
  }

  ~MonitoringWarningView() override = default;

 private:
  void UpdateLabel() {
    // Call sequence of UpdateForUser() and SetWarningType() is not clear.
    // In case SetWarningType is called first there is a need to wait for
    // enterprise_domain_ is set.
    if (warning_type_ == WarningType::kNone || !enterprise_domain_.has_value())
      return;
    base::string16 label_text;
    if (warning_type_ == WarningType::kFullWarning) {
      label_text = l10n_util::GetStringFUTF16(
          IDS_ASH_LOGIN_MANAGED_SESSION_MONITORING_FULL_WARNING,
          base::UTF8ToUTF16(enterprise_domain_.value()));
      image_->SetVisible(true);
    } else {
      label_text = l10n_util::GetStringFUTF16(
          IDS_ASH_LOGIN_MANAGED_SESSION_MONITORING_SOFT_WARNING,
          base::UTF8ToUTF16(enterprise_domain_.value()));
      image_->SetVisible(false);
    }
    label_->SetText(label_text);
  }

  friend class LoginExpandedPublicAccountView::TestApi;

  WarningType warning_type_;
  base::Optional<std::string> enterprise_domain_;
  views::ImageView* image_;
  views::Label* label_;

  DISALLOW_COPY_AND_ASSIGN(MonitoringWarningView);
};

// Implements the right part of the expanded public session view.
class RightPaneView : public NonAccessibleView,
                      public views::ButtonListener,
                      public views::StyledLabelListener {
 public:
  explicit RightPaneView(const base::RepeatingClosure& on_learn_more_tapped)
      : on_learn_more_tapped_(on_learn_more_tapped) {
    SetPreferredSize(
        gfx::Size(kExpandedViewWidthDp / 2, kExpandedViewHeightDp));
    SetBorder(views::CreateEmptyBorder(gfx::Insets(kRightPaneMarginDp)));

    // Create labels view.
    labels_view_ = new NonAccessibleView();
    labels_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(),
        kSpacingBetweenLabelsDp));
    AddChildView(labels_view_);

    monitoring_warning_view_ = new MonitoringWarningView();
    labels_view_->AddChildView(monitoring_warning_view_);

    const base::string16 link = l10n_util::GetStringUTF16(IDS_ASH_LEARN_MORE);
    size_t offset;
    const base::string16 text = l10n_util::GetStringFUTF16(
        IDS_ASH_LOGIN_PUBLIC_ACCOUNT_SIGNOUT_REMINDER, link, &offset);
    learn_more_label_ = new views::StyledLabel(text, this);

    views::StyledLabel::RangeStyleInfo style;
    style.custom_font = learn_more_label_->GetDefaultFontList().Derive(
        0, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::NORMAL);
    style.override_color = SK_ColorWHITE;
    learn_more_label_->AddStyleRange(gfx::Range(0, offset), style);

    views::StyledLabel::RangeStyleInfo link_style =
        views::StyledLabel::RangeStyleInfo::CreateForLink();
    link_style.override_color = kPublicSessionBlueColor;
    learn_more_label_->AddStyleRange(gfx::Range(offset, offset + link.length()),
                                     link_style);
    learn_more_label_->SetAutoColorReadabilityEnabled(false);

    labels_view_->AddChildView(learn_more_label_);

    // Create button to show/hide advanced view.
    advanced_view_button_ = new SelectionButtonView(
        l10n_util::GetStringUTF16(
            IDS_ASH_LOGIN_PUBLIC_SESSION_LANGUAGE_AND_INPUT),
        this);
    advanced_view_button_->SetTextColor(kPublicSessionBlueColor);
    advanced_view_button_->SetIcon(kLoginScreenButtonDropdownIcon,
                                   kPublicSessionBlueColor);
    advanced_view_button_->SetPreferredSize(
        gfx::Size(kAdvancedViewButtonWidthDp, kAdvancedViewButtonHeightDp));
    AddChildView(advanced_view_button_);

    // Create advanced view.
    advanced_view_ = new NonAccessibleView();
    advanced_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
    AddChildView(advanced_view_);

    // Creates button to open the menu.
    auto create_menu_button =
        [&](const base::string16& text) -> SelectionButtonView* {
      auto* button = new SelectionButtonView(text, this);
      button->SetPreferredSize(
          gfx::Size(kSelectionBoxWidthDp, kSelectionBoxHeightDp));
      button->SetMargins(kLeftMarginForSelectionButton,
                         kRightMarginForSelectionButton);
      button->SetBorder(views::CreateRoundedRectBorder(
          kBorderThicknessDp, kRoundRectCornerRadiusDp, kBorderColor));
      button->SetIcon(kLoginScreenMenuDropdownIcon, kSelectionMenuTitleColor);
      return button;
    };

    auto create_padding = [](int amount) -> views::View* {
      auto* padding = new NonAccessibleView();
      padding->SetPreferredSize(gfx::Size(kNonEmptyWidth, amount));
      return padding;
    };

    views::Label* language_title = CreateLabel(
        l10n_util::GetStringUTF16(IDS_ASH_LOGIN_LANGUAGE_SELECTION_SELECT),
        kSelectionMenuTitleColor);
    language_selection_ = create_menu_button(base::string16());

    views::Label* keyboard_title = CreateLabel(
        l10n_util::GetStringUTF16(IDS_ASH_LOGIN_KEYBOARD_SELECTION_SELECT),
        kSelectionMenuTitleColor);
    keyboard_selection_ = create_menu_button(base::string16());

    advanced_view_->AddChildView(language_title);
    advanced_view_->AddChildView(
        create_padding(kSpacingBetweenSelectionTitleAndButtonDp));
    advanced_view_->AddChildView(language_selection_);
    advanced_view_->AddChildView(
        create_padding(kSpacingBetweenSelectionMenusDp));
    advanced_view_->AddChildView(keyboard_title);
    advanced_view_->AddChildView(
        create_padding(kSpacingBetweenSelectionTitleAndButtonDp));
    advanced_view_->AddChildView(keyboard_selection_);

    submit_button_ = new ArrowButtonView(this, kArrowButtonSizeDp);
    submit_button_->SetBackgroundColor(kArrowButtonColor);
    AddChildView(submit_button_);
  }

  ~RightPaneView() override = default;

  void Layout() override {
    const gfx::Rect bounds = GetContentsBounds();
    // Place the labels.
    const int label_width = bounds.width() - kLabelMarginDp;
    labels_view_->SetBounds(
        bounds.x(),
        show_advanced_view_ ? kTopSpacingForLabelInAdvancedViewDp + bounds.y()
                            : kTopSpacingForLabelInRegularViewDp + bounds.y(),
        label_width, labels_view_->GetHeightForWidth(label_width));

    // Place the advanced_view_button_.
    advanced_view_button_->SizeToPreferredSize();
    advanced_view_button_->SetPosition(gfx::Point(
        bounds.x(),
        show_advanced_view_
            ? labels_view_->bounds().bottom() + kSpacingBetweenLabelsDp
            : labels_view_->bounds().bottom() +
                  kSpacingBetweenAdvancedViewButtonAndLabelDp));

    // Place the advanced view.
    advanced_view_->SetVisible(show_advanced_view_);
    if (show_advanced_view_) {
      advanced_view_->SizeToPreferredSize();
      advanced_view_->SetPosition(gfx::Point(
          bounds.x(), advanced_view_button_->bounds().bottom() +
                          kSpacingBetweenLanguageMenuAndAdvancedViewButtonDp));
    }

    // Place the submit button.
    submit_button_->SizeToPreferredSize();
    submit_button_->SetPosition(
        gfx::Point(bounds.right() - kArrowButtonSizeDp,
                   bounds.bottom() - kArrowButtonSizeDp));
  }

  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    if (sender == advanced_view_button_) {
      show_advanced_view_ = !show_advanced_view_;
      show_advanced_changed_by_user_ = true;
      Layout();
    } else if (sender == submit_button_) {
      // TODO(crbug.com/984021) change to LaunchSamlPublicSession which would
      // take selected_language_item_.value, selected_keyboard_item_.value too.
      if (current_user_.public_account_info->using_saml) {
        Shell::Get()->login_screen_controller()->ShowGaiaSignin(
            true /*can_close*/, current_user_.basic_user_info.account_id);
      } else {
        Shell::Get()->login_screen_controller()->LaunchPublicSession(
            current_user_.basic_user_info.account_id,
            selected_language_item_.value, selected_keyboard_item_.value);
      }
    } else if (sender == language_selection_) {
      DCHECK(language_menu_view_);
      if (language_menu_view_->GetVisible()) {
        language_menu_view_->Hide();
      } else {
        bool opener_had_focus = language_selection_->HasFocus();

        language_menu_view_->Show();

        if (opener_had_focus)
          language_menu_view_->RequestFocus();
      }
    } else if (sender == keyboard_selection_) {
      DCHECK(keyboard_menu_view_);
      if (keyboard_menu_view_->GetVisible()) {
        keyboard_menu_view_->Hide();
      } else {
        bool opener_had_focus = keyboard_selection_->HasFocus();

        keyboard_menu_view_->Show();

        if (opener_had_focus)
          keyboard_menu_view_->RequestFocus();
      }
    }
  }

  // "Learn more" is clicked to show additional information of what the device
  // admin may monitor.
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override {
    on_learn_more_tapped_.Run();
  }

  void UpdateForUser(const LoginUserInfo& user) {
    DCHECK_EQ(user.basic_user_info.type,
              user_manager::USER_TYPE_PUBLIC_ACCOUNT);
    monitoring_warning_view_->UpdateForUser(user);
    current_user_ = user;
    if (!language_changed_by_user_)
      selected_language_item_.value = user.public_account_info->default_locale;

    PopulateLanguageItems(user.public_account_info->available_locales);
    PopulateKeyboardItems(user.public_account_info->keyboard_layouts);
    language_selection_->SetText(
        base::UTF8ToUTF16(selected_language_item_.title));
    keyboard_selection_->SetText(
        base::UTF8ToUTF16(selected_keyboard_item_.title));

    if (!show_advanced_changed_by_user_)
      show_advanced_view_ = user.public_account_info->show_advanced_view;

    Layout();
  }

  void SetShowFullManagementDisclosure(bool show_full_management_disclosure) {
    monitoring_warning_view_->SetWarningType(
        show_full_management_disclosure
            ? MonitoringWarningView::WarningType::kFullWarning
            : MonitoringWarningView::WarningType::kSoftWarning);
  }

  void OnLanguageSelected(LoginMenuView::Item item) {
    language_changed_by_user_ = true;
    selected_language_item_ = item;
    language_selection_->SetText(base::UTF8ToUTF16(item.title));
    current_user_.public_account_info->default_locale = item.value;

    // User changed the preferred locale, request to get corresponding keyboard
    // layouts.
    Shell::Get()
        ->login_screen_controller()
        ->RequestPublicSessionKeyboardLayouts(
            current_user_.basic_user_info.account_id, item.value);
  }

  void OnKeyboardSelected(LoginMenuView::Item item) {
    selected_keyboard_item_ = item;
    keyboard_selection_->SetText(base::UTF8ToUTF16(item.title));
  }

  void PopulateLanguageItems(const std::vector<LocaleItem>& locales) {
    language_items_.clear();
    for (const auto& locale : locales) {
      LoginMenuView::Item item;
      if (locale.group_name) {
        item.title = locale.group_name.value();
        item.is_group = true;
      } else {
        item.title = locale.title;
        item.value = locale.language_code;
        item.is_group = false;
        item.selected = selected_language_item_.value == locale.language_code;
      }
      language_items_.push_back(item);

      if (selected_language_item_.value == locale.language_code)
        selected_language_item_ = item;
    }

    LoginMenuView* old_language_menu_view = language_menu_view_;

    language_menu_view_ = new LoginMenuView(
        language_items_, language_selection_ /*anchor_view*/,
        language_selection_ /*bubble_opener*/,
        base::BindRepeating(&RightPaneView::OnLanguageSelected,
                            weak_factory_.GetWeakPtr()));
    login_views_utils::GetBubbleContainer(this)->AddChildView(
        language_menu_view_);

    if (old_language_menu_view)
      delete old_language_menu_view;
  }

  void PopulateKeyboardItems(
      const std::vector<InputMethodItem>& keyboard_layouts) {
    keyboard_items_.clear();
    for (const auto& keyboard : keyboard_layouts) {
      LoginMenuView::Item item;
      item.title = keyboard.title;
      item.value = keyboard.ime_id;
      item.is_group = false;
      item.selected = keyboard.selected;
      keyboard_items_.push_back(item);

      if (keyboard.selected)
        selected_keyboard_item_ = item;
    }

    LoginMenuView* old_keyboard_menu_view = keyboard_menu_view_;

    keyboard_menu_view_ = new LoginMenuView(
        keyboard_items_, keyboard_selection_ /*anchor_view*/,
        keyboard_selection_ /*bubble_opener*/,
        base::BindRepeating(&RightPaneView::OnKeyboardSelected,
                            weak_factory_.GetWeakPtr()));
    login_views_utils::GetBubbleContainer(this)->AddChildView(
        keyboard_menu_view_);

    if (old_keyboard_menu_view)
      delete old_keyboard_menu_view;
  }

  LoginBaseBubbleView* GetLanguageMenuView() { return language_menu_view_; }

  LoginBaseBubbleView* GetKeyboardMenuView() { return keyboard_menu_view_; }

  // Close language and keyboard menus and reset local states.
  void Reset() {
    show_advanced_changed_by_user_ = false;
    language_changed_by_user_ = false;
    if (language_menu_view_ && language_menu_view_->GetVisible())
      language_menu_view_->Hide();
    if (keyboard_menu_view_ && keyboard_menu_view_->GetVisible())
      keyboard_menu_view_->Hide();
  }

 private:
  friend class LoginExpandedPublicAccountView::TestApi;

  bool show_advanced_view_ = false;
  LoginUserInfo current_user_;

  views::View* labels_view_ = nullptr;
  SelectionButtonView* advanced_view_button_ = nullptr;
  views::View* advanced_view_ = nullptr;
  SelectionButtonView* language_selection_ = nullptr;
  SelectionButtonView* keyboard_selection_ = nullptr;
  ArrowButtonView* submit_button_ = nullptr;
  views::StyledLabel* learn_more_label_ = nullptr;
  MonitoringWarningView* monitoring_warning_view_ = nullptr;

  // |language_menu_view_| and |keyboard_menu_view_| are parented by the top
  // level view, either LockContentsView or LockDebugView. This allows the menu
  // items to be clicked outside the bounds of the right pane view.
  LoginMenuView* language_menu_view_ = nullptr;
  LoginMenuView* keyboard_menu_view_ = nullptr;

  LoginMenuView::Item selected_language_item_;
  LoginMenuView::Item selected_keyboard_item_;
  std::vector<LoginMenuView::Item> language_items_;
  std::vector<LoginMenuView::Item> keyboard_items_;

  // Local states to check if the locale and whether to show advanced view
  // has been changed by the user. This ensures user action won't be overridden
  // by the default settings and it will be reset after this view is hidden.
  // Keyboard selection is not tracked here because it's depending on which
  // locale user selects, so the previously selected keyboard might not be
  // applicable for the current locale.
  bool show_advanced_changed_by_user_ = false;
  bool language_changed_by_user_ = false;

  base::RepeatingClosure on_learn_more_tapped_;

  base::WeakPtrFactory<RightPaneView> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RightPaneView);
};

LoginExpandedPublicAccountView::TestApi::TestApi(
    LoginExpandedPublicAccountView* view)
    : view_(view) {}

LoginExpandedPublicAccountView::TestApi::~TestApi() = default;

views::View* LoginExpandedPublicAccountView::TestApi::advanced_view_button() {
  return view_->right_pane_->advanced_view_button_;
}

ArrowButtonView* LoginExpandedPublicAccountView::TestApi::submit_button() {
  return view_->right_pane_->submit_button_;
}

views::View* LoginExpandedPublicAccountView::TestApi::advanced_view() {
  return view_->right_pane_->advanced_view_;
}

PublicAccountWarningDialog*
LoginExpandedPublicAccountView::TestApi::warning_dialog() {
  return view_->warning_dialog_;
}

views::StyledLabel*
LoginExpandedPublicAccountView::TestApi::learn_more_label() {
  return view_->right_pane_->learn_more_label_;
}

views::View*
LoginExpandedPublicAccountView::TestApi::language_selection_button() {
  return view_->right_pane_->language_selection_;
}

views::View*
LoginExpandedPublicAccountView::TestApi::keyboard_selection_button() {
  return view_->right_pane_->keyboard_selection_;
}

LoginMenuView* LoginExpandedPublicAccountView::TestApi::language_menu_view() {
  return view_->right_pane_->language_menu_view_;
}

LoginMenuView* LoginExpandedPublicAccountView::TestApi::keyboard_menu_view() {
  return view_->right_pane_->keyboard_menu_view_;
}

LoginMenuView::Item
LoginExpandedPublicAccountView::TestApi::selected_language_item() {
  return view_->right_pane_->selected_language_item_;
}

LoginMenuView::Item
LoginExpandedPublicAccountView::TestApi::selected_keyboard_item() {
  return view_->right_pane_->selected_keyboard_item_;
}

views::ImageView*
LoginExpandedPublicAccountView::TestApi::monitoring_warning_icon() {
  return view_->right_pane_->monitoring_warning_view_->image_;
}

views::Label*
LoginExpandedPublicAccountView::TestApi::monitoring_warning_label() {
  return view_->right_pane_->monitoring_warning_view_->label_;
}

void LoginExpandedPublicAccountView::TestApi::ResetUserForTest() {
  view_->right_pane_->monitoring_warning_view_->enterprise_domain_.reset();
}

LoginExpandedPublicAccountView::LoginExpandedPublicAccountView(
    const OnPublicSessionViewDismissed& on_dismissed)
    : NonAccessibleView(kLoginExpandedPublicAccountViewClassName),
      on_dismissed_(on_dismissed),
      event_handler_(
          std::make_unique<LoginExpandedPublicAccountEventHandler>(this)) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));
  SetPreferredSize(gfx::Size(kExpandedViewWidthDp, kExpandedViewHeightDp));

  user_view_ = new LoginUserView(
      LoginDisplayStyle::kLarge, false /*show_dropdown*/, true /*show_domain*/,
      base::DoNothing(), base::RepeatingClosure(), base::RepeatingClosure());
  user_view_->SetForceOpaque(true);
  user_view_->SetTapEnabled(false);

  auto* left_pane = new NonAccessibleView();
  AddChildView(left_pane);
  left_pane->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  left_pane->SetPreferredSize(
      gfx::Size(kExpandedViewWidthDp / 2, kExpandedViewHeightDp));

  auto* top_spacing = new NonAccessibleView();
  top_spacing->SetPreferredSize(
      gfx::Size(kNonEmptyWidth, kTopSpacingForUserViewDp));
  left_pane->AddChildView(top_spacing);
  left_pane->AddChildView(user_view_);
  left_pane->SetBorder(
      views::CreateSolidSidedBorder(0, 0, 0, kBorderThicknessDp, kBorderColor));

  right_pane_ = new RightPaneView(
      base::BindRepeating(&LoginExpandedPublicAccountView::ShowWarningDialog,
                          base::Unretained(this)));
  AddChildView(right_pane_);
}

LoginExpandedPublicAccountView::~LoginExpandedPublicAccountView() = default;

void LoginExpandedPublicAccountView::ProcessPressedEvent(
    const ui::LocatedEvent* event) {
  if (!GetVisible())
    return;

  // Keep this view to be visible until warning dialog is dismissed.
  if (warning_dialog_ && warning_dialog_->IsVisible())
    return;

  if (GetBoundsInScreen().Contains(event->root_location()))
    return;

  // Ignore press event inside the language and keyboard menu.
  LoginBaseBubbleView* language_menu_view = right_pane_->GetLanguageMenuView();
  if (language_menu_view &&
      language_menu_view->GetBoundsInScreen().Contains(event->root_location()))
    return;

  LoginBaseBubbleView* keyboard_menu_view = right_pane_->GetKeyboardMenuView();
  if (keyboard_menu_view &&
      keyboard_menu_view->GetBoundsInScreen().Contains(event->root_location()))
    return;

  Hide();
}

void LoginExpandedPublicAccountView::UpdateForUser(const LoginUserInfo& user) {
  user_view_->UpdateForUser(user, false /*animate*/);
  right_pane_->UpdateForUser(user);
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
  DCHECK(!warning_dialog_);
  warning_dialog_ = new PublicAccountWarningDialog(weak_factory_.GetWeakPtr());
  warning_dialog_->Show();
}

void LoginExpandedPublicAccountView::OnWarningDialogClosed() {
  warning_dialog_ = nullptr;
}

void LoginExpandedPublicAccountView::SetShowFullManagementDisclosure(
    bool show_full_management_disclosure) {
  right_pane_->SetShowFullManagementDisclosure(show_full_management_disclosure);
}

void LoginExpandedPublicAccountView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(kPublicSessionBackgroundColor);
  flags.setAntiAlias(true);
  canvas->DrawRoundRect(GetContentsBounds(), kRoundRectCornerRadiusDp, flags);
}

void LoginExpandedPublicAccountView::OnKeyEvent(ui::KeyEvent* event) {
  if (!GetVisible() || event->type() != ui::ET_KEY_PRESSED)
    return;

  // Give warning dialog a chance to handle key event.
  if (warning_dialog_ && warning_dialog_->IsVisible())
    return;

  if (event->key_code() == ui::KeyboardCode::VKEY_ESCAPE) {
    Hide();
  }
}

}  // namespace ash
