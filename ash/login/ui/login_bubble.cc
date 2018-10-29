// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_bubble.h"

#include <memory>
#include <utility>

#include "ash/focus_cycler.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/lock_window.h"
#include "ash/login/ui/login_button.h"
#include "ash/login/ui/login_menu_view.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/client/focus_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/font.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace {

constexpr char kLegacySupervisedUserManagementDisplayURL[] =
    "www.chrome.com/manage";

// Spacing between the child view inside the bubble view.
constexpr int kBubbleBetweenChildSpacingDp = 6;

// The size of the alert icon in the error bubble.
constexpr int kAlertIconSizeDp = 20;

// An alpha value for the sub message in the user menu.
constexpr SkAlpha kSubMessageColorAlpha = 0x89;

// Color of the "Remove user" text.
constexpr SkColor kRemoveUserInitialColor = gfx::kGoogleBlueDark400;
constexpr SkColor kRemoveUserConfirmColor = gfx::kGoogleRedDark500;

// Margin/inset of the entries for the user menu.
constexpr int kUserMenuMarginWidth = 14;
constexpr int kUserMenuMarginHeight = 18;
// Distance above/below the separator.
constexpr int kUserMenuMarginAroundSeparatorDp = 18;
// Distance between labels.
constexpr int kUserMenuVerticalDistanceBetweenLabelsDp = 18;
// Margin around remove user button.
constexpr int kUserMenuMarginAroundRemoveUserButtonDp = 4;

// Vertical spacing between the anchor view and error bubble.
constexpr int kAnchorViewErrorBubbleVerticalSpacingDp = 48;

// Horizontal spacing with the anchor view.
constexpr int kAnchorViewUserMenuHorizontalSpacingDp = 98;

// Vertical spacing between the anchor view and user menu.
constexpr int kAnchorViewUserMenuVerticalSpacingDp = 4;

// The amount of time for bubble show/hide animation.
constexpr int kBubbleAnimationDurationMs = 300;

views::Label* CreateLabel(const base::string16& message, SkColor color) {
  views::Label* label = new views::Label(message, views::style::CONTEXT_LABEL,
                                         views::style::STYLE_PRIMARY);
  label->SetAutoColorReadabilityEnabled(false);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetEnabledColor(color);
  label->SetSubpixelRenderingEnabled(false);
  const gfx::FontList& base_font_list = views::Label::GetDefaultFontList();
  label->SetFontList(base_font_list.Derive(0, gfx::Font::FontStyle::NORMAL,
                                           gfx::Font::Weight::NORMAL));
  return label;
}

class LoginErrorBubbleView : public LoginBaseBubbleView {
 public:
  LoginErrorBubbleView(views::View* content, views::View* anchor_view)
      : LoginBaseBubbleView(anchor_view) {
    set_anchor_view_insets(
        gfx::Insets(kAnchorViewErrorBubbleVerticalSpacingDp, 0));

    gfx::Insets margins(kUserMenuMarginHeight, kUserMenuMarginWidth);

    set_margins(gfx::Insets(0, margins.left(), 0, margins.right()));

    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::kVertical,
        gfx::Insets(margins.top(), 0, margins.bottom(), 0),
        kBubbleBetweenChildSpacingDp));

    auto* alert_view = new NonAccessibleView("AlertIconContainer");
    alert_view->SetLayoutManager(
        std::make_unique<views::BoxLayout>(views::BoxLayout::kHorizontal));
    views::ImageView* alert_icon = new views::ImageView();
    alert_icon->SetPreferredSize(gfx::Size(kAlertIconSizeDp, kAlertIconSizeDp));
    alert_icon->SetImage(
        gfx::CreateVectorIcon(kLockScreenAlertIcon, SK_ColorWHITE));
    alert_view->AddChildView(alert_icon);
    AddChildView(alert_view);

    AddChildView(content);
  }

  ~LoginErrorBubbleView() override = default;

  // views::View:
  const char* GetClassName() const override { return "LoginErrorBubbleView"; }
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->role = ax::mojom::Role::kTooltip;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginErrorBubbleView);
};

// A button that holds a child view.
class ButtonWithContent : public views::Button {
 public:
  ButtonWithContent(views::ButtonListener* listener, views::View* content)
      : views::Button(listener) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    AddChildView(content);

    // Increase the size of the button so that the focus is not rendered next to
    // the text.
    SetBorder(views::CreateEmptyBorder(
        gfx::Insets(kUserMenuMarginAroundRemoveUserButtonDp,
                    kUserMenuMarginAroundRemoveUserButtonDp)));
    SetFocusPainter(views::Painter::CreateSolidFocusPainter(
        kFocusBorderColor, kFocusBorderThickness, gfx::InsetsF()));
  }

  ~ButtonWithContent() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ButtonWithContent);
};

// A view that has a customizable accessible name.
class ViewWithAccessibleName : public views::View {
 public:
  ViewWithAccessibleName(const base::string16& accessible_name)
      : accessible_name_(accessible_name) {}
  ~ViewWithAccessibleName() override = default;

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->role = ax::mojom::Role::kStaticText;
    node_data->SetName(accessible_name_);
  }

 private:
  const base::string16 accessible_name_;
  DISALLOW_COPY_AND_ASSIGN(ViewWithAccessibleName);
};

class LoginUserMenuView : public LoginBaseBubbleView,
                          public views::ButtonListener {
 public:
  LoginUserMenuView(LoginBubble* bubble,
                    const base::string16& username,
                    const base::string16& email,
                    user_manager::UserType type,
                    bool is_owner,
                    views::View* anchor_view,
                    bool show_remove_user,
                    base::OnceClosure on_remove_user_warning_shown,
                    base::OnceClosure on_remove_user_requested)
      : LoginBaseBubbleView(anchor_view),
        bubble_(bubble),
        on_remove_user_warning_shown_(std::move(on_remove_user_warning_shown)),
        on_remove_user_requested_(std::move(on_remove_user_requested)) {
    // This view has content the user can interact with if the remove user
    // button is displayed.
    set_can_activate(show_remove_user);

    set_anchor_view_insets(gfx::Insets(kAnchorViewUserMenuVerticalSpacingDp,
                                       kAnchorViewUserMenuHorizontalSpacingDp));

    // LoginUserMenuView does not use the parent margins. Further, because the
    // splitter spans the entire view set_margins cannot be used.
    set_margins(gfx::Insets());
    // The bottom margin is less the margin around the remove user button, which
    // is always visible.
    gfx::Insets margins(
        kUserMenuMarginHeight, kUserMenuMarginWidth,
        kUserMenuMarginHeight - kUserMenuMarginAroundRemoveUserButtonDp,
        kUserMenuMarginWidth);
    auto setup_horizontal_margin_container = [&](views::View* container) {
      container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::kVertical,
          gfx::Insets(0, margins.left(), 0, margins.right())));
      AddChildView(container);
      return container;
    };

    // Add vertical whitespace.
    auto add_space = [](views::View* root, int amount) {
      auto* spacer = new NonAccessibleView("Whitespace");
      spacer->SetPreferredSize(gfx::Size(1, amount));
      root->AddChildView(spacer);
    };

    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::kVertical,
        gfx::Insets(margins.top(), 0, margins.bottom(), 0)));

    // User information.
    {
      base::string16 display_username =
          is_owner ? l10n_util::GetStringFUTF16(IDS_ASH_LOGIN_POD_OWNER_USER,
                                                username)
                   : username;

      views::View* container = setup_horizontal_margin_container(
          new NonAccessibleView("UsernameLabel MarginContainer"));
      username_label_ = CreateLabel(display_username, SK_ColorWHITE);
      // Do not change these two lines. Without them, the remove user button
      // will be pushed out of the box when the user has a long name.
      username_label_->SetMultiLine(true);
      username_label_->SetMaxLines(1);
      container->AddChildView(username_label_);
      add_space(container, kBubbleBetweenChildSpacingDp);
      container->AddChildView(CreateLabel(
          email, SkColorSetA(SK_ColorWHITE, kSubMessageColorAlpha)));
    }

    // Remove user.
    if (show_remove_user) {
      DCHECK(!is_owner);

      // Add separator.
      add_space(this, kUserMenuMarginAroundSeparatorDp);
      auto* separator = new views::Separator();
      separator->SetColor(SkColorSetA(SK_ColorWHITE, 0x2B));
      AddChildView(separator);
      // The space below the separator is less the margin around remove user;
      // this is readded if showing confirmation.
      add_space(this, kUserMenuMarginAroundSeparatorDp -
                          kUserMenuMarginAroundRemoveUserButtonDp);

      auto make_label = [this](const base::string16& text) {
        views::Label* label = CreateLabel(text, SK_ColorWHITE);
        label->SetMultiLine(true);
        label->SetAllowCharacterBreak(true);
        // Make sure to set a maximum label width, otherwise text wrapping will
        // significantly increase width and layout may not work correctly if
        // the input string is very long.
        label->SetMaximumWidth(GetPreferredSize().width());
        return label;
      };

      base::string16 part1 = l10n_util::GetStringUTF16(
          IDS_ASH_LOGIN_POD_NON_OWNER_USER_REMOVE_WARNING_PART_1);
      if (type == user_manager::UserType::USER_TYPE_SUPERVISED) {
        part1 = l10n_util::GetStringFUTF16(
            IDS_ASH_LOGIN_POD_LEGACY_SUPERVISED_USER_REMOVE_WARNING,
            base::UTF8ToUTF16(ash::kLegacySupervisedUserManagementDisplayURL));
      }
      base::string16 part2 = l10n_util::GetStringFUTF16(
          IDS_ASH_LOGIN_POD_NON_OWNER_USER_REMOVE_WARNING_PART_2, email);

      remove_user_confirm_data_ = setup_horizontal_margin_container(
          new ViewWithAccessibleName(part1 + base::ASCIIToUTF16(" ") + part2));
      remove_user_confirm_data_->SetVisible(false);

      // Account for margin that was removed below the separator for the add
      // user button.
      add_space(remove_user_confirm_data_,
                kUserMenuMarginAroundRemoveUserButtonDp);
      remove_user_confirm_data_->AddChildView(make_label(part1));
      add_space(remove_user_confirm_data_,
                kUserMenuVerticalDistanceBetweenLabelsDp);
      remove_user_confirm_data_->AddChildView(make_label(part2));
      // Reduce margin since the remove user button comes next.
      add_space(remove_user_confirm_data_,
                kUserMenuVerticalDistanceBetweenLabelsDp -
                    kUserMenuMarginAroundRemoveUserButtonDp);

      auto* container = setup_horizontal_margin_container(
          new NonAccessibleView("RemoveUserButton MarginContainer"));
      remove_user_label_ =
          CreateLabel(l10n_util::GetStringUTF16(
                          IDS_ASH_LOGIN_POD_MENU_REMOVE_ITEM_ACCESSIBLE_NAME),
                      kRemoveUserInitialColor);
      remove_user_button_ = new ButtonWithContent(this, remove_user_label_);
      remove_user_button_->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
      remove_user_button_->set_id(
          LoginBubble::kUserMenuRemoveUserButtonIdForTest);
      remove_user_button_->SetAccessibleName(remove_user_label_->text());
      container->AddChildView(remove_user_button_);
    }

    // The user menu is focusable so that the we can detect when to refocus the
    // lock window from tab navigation, otherwise focus will be trapped inside
    // of the bubble.
    SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  }

  ~LoginUserMenuView() override = default;

  // views::View:
  const char* GetClassName() const override { return "LoginUserMenuView"; }
  gfx::Size CalculatePreferredSize() const override {
    gfx::Size size = LoginBaseBubbleView::CalculatePreferredSize();
    // We don't use margins() directly which means that we need to account for
    // the margin width here. Margin height is accounted for by the layout code.
    size.Enlarge(kUserMenuMarginWidth, 0);
    return size;
  }
  void OnFocus() override {
    // This view has no actual interesting contents to focus, so immediately
    // forward to the button.
    remove_user_button_->RequestFocus();
  }
  void AboutToRequestFocusFromTabTraversal(bool reverse) override {
    // Redirect the focus event to the lock screen.
    Shell::Get()->focus_cycler()->FocusWidget(LockScreen::Get()->window());
    LockScreen::Get()->window()->GetFocusManager()->AdvanceFocus(reverse);
  }

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    // Show confirmation warning. The user has to click the button again before
    // we actually allow the exit.
    if (!remove_user_confirm_data_->visible()) {
      remove_user_confirm_data_->SetVisible(true);
      remove_user_label_->SetEnabledColor(kRemoveUserConfirmColor);
      SetSize(GetPreferredSize());
      SizeToContents();
      Layout();

      // Fire an accessibility alert to make ChromeVox read the warning message
      // and remove button.
      remove_user_confirm_data_->NotifyAccessibilityEvent(
          ax::mojom::Event::kAlert, true /*send_native_event*/);
      remove_user_button_->NotifyAccessibilityEvent(ax::mojom::Event::kAlert,
                                                    true /*send_native_event*/);

      if (on_remove_user_warning_shown_)
        std::move(on_remove_user_warning_shown_).Run();
      return;
    }

    // Close the bubble before calling |on_remove_user_requested_|. |bubble_| is
    // an unowned reference; |on_remove_user_requested_| may delete it.
    bubble_->Close();

    if (on_remove_user_requested_)
      std::move(on_remove_user_requested_).Run();
  }

  views::View* remove_user_button() { return remove_user_button_; }

  views::View* remove_user_confirm_data() { return remove_user_confirm_data_; }

  views::Label* username_label() { return username_label_; }

 private:
  LoginBubble* bubble_ = nullptr;
  base::OnceClosure on_remove_user_warning_shown_;
  base::OnceClosure on_remove_user_requested_;
  views::View* remove_user_confirm_data_ = nullptr;
  views::Label* remove_user_label_ = nullptr;
  ButtonWithContent* remove_user_button_ = nullptr;
  views::Label* username_label_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(LoginUserMenuView);
};

class LoginTooltipView : public LoginBaseBubbleView {
 public:
  LoginTooltipView(const base::string16& message, views::View* anchor_view)
      : LoginBaseBubbleView(anchor_view) {
    SetLayoutManager(
        std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));
    views::Label* text = CreateLabel(message, SK_ColorWHITE);
    text->SetMultiLine(true);
    AddChildView(text);
  }

  // LoginBaseBubbleView:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->role = ax::mojom::Role::kTooltip;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginTooltipView);
};

}  // namespace

// static
const int LoginBubble::kUserMenuRemoveUserButtonIdForTest = 1;

LoginBubble::TestApi::TestApi(LoginBaseBubbleView* bubble_view)
    : bubble_view_(bubble_view) {}

views::View* LoginBubble::TestApi::user_menu_remove_user_button() {
  return static_cast<LoginUserMenuView*>(bubble_view_)->remove_user_button();
}

views::View* LoginBubble::TestApi::remove_user_confirm_data() {
  return static_cast<LoginUserMenuView*>(bubble_view_)
      ->remove_user_confirm_data();
}

views::Label* LoginBubble::TestApi::username_label() {
  return static_cast<LoginUserMenuView*>(bubble_view_)->username_label();
}

LoginBubble::LoginBubble() {
  Shell::Get()->AddPreTargetHandler(this);
}

LoginBubble::~LoginBubble() {
  Shell::Get()->RemovePreTargetHandler(this);
  if (bubble_view_)
    CloseImmediately();
}

void LoginBubble::ShowErrorBubble(views::View* content,
                                  views::View* anchor_view,
                                  uint32_t flags) {
  if (bubble_view_)
    CloseImmediately();

  flags_ = flags;
  bubble_view_ = new LoginErrorBubbleView(content, anchor_view);

  Show();
}

void LoginBubble::ShowUserMenu(const base::string16& username,
                               const base::string16& email,
                               user_manager::UserType type,
                               bool is_owner,
                               views::View* anchor_view,
                               LoginButton* bubble_opener,
                               bool show_remove_user,
                               base::OnceClosure on_remove_user_warning_shown,
                               base::OnceClosure on_remove_user_requested) {
  if (bubble_view_)
    CloseImmediately();

  flags_ = kFlagsNone;
  bubble_opener_ = bubble_opener;
  bubble_view_ = new LoginUserMenuView(this, username, email, type, is_owner,
                                       anchor_view, show_remove_user,
                                       std::move(on_remove_user_warning_shown),
                                       std::move(on_remove_user_requested));
  bool had_focus = bubble_opener_->HasFocus();
  Show();
  if (had_focus) {
    // Try to focus the bubble view only if the bubble opener was focused.
    bubble_view_->RequestFocus();
  }
}

void LoginBubble::ShowTooltip(const base::string16& message,
                              views::View* anchor_view) {
  if (bubble_view_)
    CloseImmediately();

  flags_ = kFlagsNone;
  bubble_view_ = new LoginTooltipView(message, anchor_view);
  Show();
}

void LoginBubble::ShowSelectionMenu(LoginMenuView* menu,
                                    LoginButton* bubble_opener) {
  if (bubble_view_)
    CloseImmediately();

  flags_ = kFlagsNone;
  bubble_opener_ = bubble_opener;
  const bool had_focus = bubble_opener_->HasFocus();

  // Transfer the ownership of |menu| to bubble widget.
  bubble_view_ = menu;
  Show();

  if (had_focus) {
    // Try to focus the bubble view only if the bubble opener was focused.
    bubble_view_->RequestFocus();
  }
}

void LoginBubble::Close() {
  ScheduleAnimation(false /*visible*/);
}

void LoginBubble::CloseImmediately() {
  DCHECK(bubble_view_);
  Reset(false /*widget_already_closing*/);
}

bool LoginBubble::IsVisible() {
  return bubble_view_ && bubble_view_->GetWidget()->IsVisible();
}

void LoginBubble::OnWidgetClosing(views::Widget* widget) {
  DCHECK_EQ(bubble_view_->GetWidget(), widget);
  Reset(true /*widget_already_closing*/);
}

void LoginBubble::OnWidgetDestroying(views::Widget* widget) {
  OnWidgetClosing(widget);
}

void LoginBubble::OnWidgetBoundsChanged(views::Widget* widget,
                                        const gfx::Rect& new_bounds) {
  EnsureBubbleInScreen();
}

void LoginBubble::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED)
    ProcessPressedEvent(event->AsLocatedEvent());
}

void LoginBubble::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP ||
      event->type() == ui::ET_GESTURE_TAP_DOWN) {
    ProcessPressedEvent(event->AsLocatedEvent());
  }
}

void LoginBubble::OnKeyEvent(ui::KeyEvent* event) {
  // Ignore VKEY_PROCESSKEY; it is an IME event saying that the key has been
  // processed. This event is also generated in tablet mode, ie, after
  // submitting a password a VKEY_PROCESSKEY event is generated. If we treat
  // that as a normal key event the password bubble will be dismissed
  // immediately after submitting.
  if (!bubble_view_ || event->type() != ui::ET_KEY_PRESSED ||
      event->key_code() == ui::VKEY_PROCESSKEY) {
    return;
  }

  // If current focus view is the button view, don't process the event here,
  // let the button logic handle the event and determine show/hide behavior.
  if (bubble_opener_ && bubble_opener_->HasFocus())
    return;

  // If |bubble_view_| is interactive do not close it.
  if (bubble_view_->GetWidget()->IsActive())
    return;

  if (!(flags_ & kFlagPersistent)) {
    Close();
  }
}

void LoginBubble::OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) {
  if (!bubble_view_)
    return;

  bubble_view_->layer()->GetAnimator()->RemoveObserver(this);
  if (!is_visible_)
    CloseImmediately();
}

void LoginBubble::OnWindowFocused(aura::Window* gained_focus,
                                  aura::Window* lost_focus) {
  if (!bubble_view_ || !IsVisible())
    return;

  aura::Window* bubble_window = bubble_view_->GetWidget()->GetNativeView();
  // Bubble window has the focus, do nothing.
  if (gained_focus && bubble_window->Contains(gained_focus))
    return;

  if (!(flags_ & kFlagPersistent))
    Close();
}

void LoginBubble::Show() {
  DCHECK(bubble_view_);
  views::Widget* widget =
      views::BubbleDialogDelegateView::CreateBubble(bubble_view_);
  EnsureBubbleInScreen();
  widget->ShowInactive();
  widget->AddObserver(this);
  widget->StackAtTop();
  aura::client::GetFocusClient(widget->GetNativeView())->AddObserver(this);

  ScheduleAnimation(true /*visible*/);

  // Fire an alert so ChromeVox will read the contents of the bubble.
  bubble_view_->NotifyAccessibilityEvent(ax::mojom::Event::kAlert,
                                         true /*send_native_event*/);
}

void LoginBubble::ProcessPressedEvent(const ui::LocatedEvent* event) {
  if (!bubble_view_)
    return;

  // Don't process event inside the bubble.
  gfx::Point screen_location = event->location();
  ::wm::ConvertPointToScreen(static_cast<aura::Window*>(event->target()),
                             &screen_location);
  gfx::Rect bounds = bubble_view_->GetWidget()->GetWindowBoundsInScreen();
  if (bounds.Contains(screen_location))
    return;

  // If the user clicks on the button view, don't process the event here,
  // let the button logic handle the event and determine show/hide behavior.
  if (bubble_opener_) {
    gfx::Rect bubble_opener_bounds = bubble_opener_->GetBoundsInScreen();
    if (bubble_opener_bounds.Contains(screen_location))
      return;
  }

  if (!(flags_ & kFlagPersistent))
    Close();
}

void LoginBubble::ScheduleAnimation(bool visible) {
  if (!bubble_view_ || is_visible_ == visible)
    return;

  if (bubble_opener_) {
    bubble_opener_->AnimateInkDrop(visible ? views::InkDropState::ACTIVATED
                                           : views::InkDropState::DEACTIVATED,
                                   nullptr /*event*/);
  }

  ui::Layer* layer = bubble_view_->layer();
  layer->GetAnimator()->StopAnimating();
  is_visible_ = visible;

  float opacity_start = 0.0f;
  float opacity_end = 1.0f;
  if (!is_visible_)
    std::swap(opacity_start, opacity_end);

  layer->GetAnimator()->AddObserver(this);
  layer->SetOpacity(opacity_start);
  {
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kBubbleAnimationDurationMs));
    settings.SetTweenType(is_visible_ ? gfx::Tween::EASE_OUT
                                      : gfx::Tween::EASE_IN);
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    layer->SetOpacity(opacity_end);
  }
}

void LoginBubble::Reset(bool widget_already_closing) {
  DCHECK(bubble_view_);
  aura::client::GetFocusClient(bubble_view_->GetWidget()->GetNativeView())
      ->RemoveObserver(this);
  bubble_view_->GetWidget()->RemoveObserver(this);
  bubble_view_->layer()->GetAnimator()->RemoveObserver(this);

  if (!widget_already_closing)
    bubble_view_->GetWidget()->Close();
  is_visible_ = false;
  bubble_opener_ = nullptr;
  bubble_view_ = nullptr;
  flags_ = kFlagsNone;
}

void LoginBubble::EnsureBubbleInScreen() {
  DCHECK(bubble_view_);
  DCHECK(bubble_view_->GetWidget());

  const gfx::Rect view_bounds = bubble_view_->GetBoundsInScreen();
  const gfx::Rect work_area =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(
              bubble_view_->GetWidget()->GetNativeWindow())
          .work_area();

  int horizontal_offset = 0;

  // If the widget extends past the right side of the screen, make it go to
  // the left instead.
  if (work_area.right() < view_bounds.right()) {
    horizontal_offset = -view_bounds.width();
  }

  bubble_view_->set_anchor_view_insets(
      bubble_view_->anchor_view_insets().Offset(
          gfx::Vector2d(horizontal_offset, 0)));
  bubble_view_->OnAnchorBoundsChanged();
}

}  // namespace ash
