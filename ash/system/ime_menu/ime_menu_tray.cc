// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/ime_menu/ime_menu_tray.h"

#include <memory>

#include "ash/accessibility/a11y_feature_type.h"
#include "ash/accessibility/accessibility_controller.h"
#include "ash/constants/tray_background_view_catalog.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/virtual_keyboard_controller.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/icon_button.h"
#include "ash/style/rounded_container.h"
#include "ash/style/typography.h"
#include "ash/system/ime_menu/ime_list_view.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/system/tray/system_menu_button.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/range/range.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

namespace {

// Used for testing.
const int kEmojiButtonId = 1;
const int kSettingsButtonId = 2;
const int kVoiceButtonId = 3;

// Insets for the title view (dp).
constexpr auto kTitleViewPadding =
    gfx::Insets::VH(0, kMenuEdgeEffectivePadding);

// Insets for the bubble view to fix the overlapping
// between the floating menu and the IME tray in kiosk session (dp).
constexpr auto kKioskBubbleViewPadding = gfx::Insets::TLBR(-19, 0, 27, 0);

// The scroll view has no margin when the bottom buttons are shown at the top or
// bottom to make it flush with the header and footer.
constexpr auto kQsScrollViewMargin = gfx::Insets::TLBR(0, 16, 0, 16);

// When the bottom buttons are not shown (e.g Lockscreen) we need to have a 16px
// inset on the bottom in addition to the existing insets.
constexpr auto kQsScrollViewMarginWithoutBottomButtons =
    gfx::Insets::TLBR(0, 16, 16, 16);

// Returns the height range of ImeListView.
gfx::Range GetImeListViewRange() {
  const int max_items = 5;
  const int min_items = 1;
  const int tray_item_height = kTrayPopupItemMinHeight;
  // Insets at the top and bottom of the RoundedContainer.
  const int insets = RoundedContainer::kBorderInsets.top() +
                     RoundedContainer::kBorderInsets.bottom();
  return gfx::Range(tray_item_height * min_items + insets,
                    tray_item_height * max_items + insets);
}

// Returns true if the current screen is login or lock screen.
bool IsInLoginOrLockScreen() {
  using session_manager::SessionState;
  SessionState state = Shell::Get()->session_controller()->GetSessionState();
  return state == SessionState::LOGIN_PRIMARY ||
         state == SessionState::LOCKED ||
         state == SessionState::LOGIN_SECONDARY;
}

// Returns true if the current input context type is password.
bool IsInPasswordInputContext() {
  return IMEBridge::Get()->GetCurrentInputContext().type ==
         ui::TEXT_INPUT_TYPE_PASSWORD;
}

// Returns true if it is Kiosk Session.
bool IsKioskSession() {
  return Shell::Get()->session_controller()->IsRunningInAppMode();
}

bool ShouldShowVoiceButton() {
  auto* ime_controller = Shell::Get()->ime_controller();
  const bool is_dictation_enabled =
      Shell::Get()
          ->accessibility_controller()
          ->GetFeature(A11yFeatureType::kDictation)
          .enabled();

  // Only enable voice button in IME tray if the function is enabled and
  // the accessibility dictation is not enabled in the shelf.
  return ime_controller->is_voice_enabled() && !is_dictation_enabled;
}

// Returns true if the menu should show emoji, handwriting and voice buttons
// on the bottom.
bool ShouldShowBottomButtons() {
  // Emoji, handwriting and voice input is not supported for these cases:
  // 1) third party IME extensions.
  // 2) login/lock screen.
  // 3) password input client.
  auto* ime_controller = Shell::Get()->ime_controller();
  bool bottom_buttons_enabled =
      ime_controller->is_extra_input_options_enabled() &&
      !ime_controller->current_ime().third_party && !IsInLoginOrLockScreen() &&
      !IsInPasswordInputContext();
  if (!bottom_buttons_enabled) {
    return false;
  }

  return ime_controller->is_emoji_enabled() ||
         ime_controller->is_handwriting_enabled() || ShouldShowVoiceButton();
}

class ImeMenuLabel : public views::Label {
  METADATA_HEADER(ImeMenuLabel, views::Label)

 public:
  ImeMenuLabel() {
    // Sometimes the label will be more than 2 characters, e.g. INTL and EXTD.
    // This border makes sure we only leave room for ~2 and the others are
    // truncated.
    SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(0, 6)));
  }
  ImeMenuLabel(const ImeMenuLabel&) = delete;
  ImeMenuLabel& operator=(const ImeMenuLabel&) = delete;
  ~ImeMenuLabel() override = default;

  // views:Label:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return gfx::Size(kTrayItemSize, kTrayItemSize);
  }
};

BEGIN_METADATA(ImeMenuLabel)
END_METADATA

class ImeMenuImageView : public views::ImageView {
  METADATA_HEADER(ImeMenuImageView, views::ImageView)

 public:
  ImeMenuImageView() {
    SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(0, 6)));
  }
  ImeMenuImageView(const ImeMenuImageView&) = delete;
  ImeMenuImageView& operator=(const ImeMenuImageView&) = delete;
  ~ImeMenuImageView() override = default;
};

BEGIN_METADATA(ImeMenuImageView)
END_METADATA

// The view that contains IME menu title.
class ImeTitleView : public views::BoxLayoutView {
  METADATA_HEADER(ImeTitleView, views::BoxLayoutView)

 public:
  ImeTitleView() {
    SetID(VIEW_ID_IME_TITLE_VIEW);
    SetOrientation(views::BoxLayout::Orientation::kHorizontal);
    SetInsideBorderInsets(kTitleViewPadding);
    SetMinimumCrossAxisSize(kTrayPopupItemMinHeight);
    SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);

    auto* title_label = AddChildView(std::make_unique<views::Label>(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_IME)));
    title_label->SetBorder(
        views::CreateEmptyBorder(gfx::Insets::TLBR(0, 0, 1, 0)));
    title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    title_label->SetEnabledColorId(kColorAshTextColorPrimary);
    title_label->SetAutoColorReadabilityEnabled(false);
    TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosTitle1,
                                          *title_label);
    SetFlexForView(title_label, 1);

    // Don't create Settings Button if it is Kiosk session.
    if (!IsKioskSession()) {
      settings_button_ = AddChildView(std::make_unique<IconButton>(
          base::BindRepeating([]() {
            base::RecordAction(
                base::UserMetricsAction("StatusArea_IME_Detailed"));
            Shell::Get()->system_tray_model()->client()->ShowIMESettings();
          }),
          IconButton::Type::kMedium, &kSystemMenuSettingsIcon,
          IDS_ASH_STATUS_TRAY_IME_SETTINGS));
      settings_button_->SetEnabled(TrayPopupUtils::CanOpenWebUISettings());
      settings_button_->SetID(kSettingsButtonId);
    }
  }
  ImeTitleView(const ImeTitleView&) = delete;
  ImeTitleView& operator=(const ImeTitleView&) = delete;

  ~ImeTitleView() override = default;

 private:
  raw_ptr<IconButton> settings_button_ = nullptr;
};

BEGIN_METADATA(ImeTitleView)
END_METADATA

// The view that contains buttons shown on the bottom of IME menu.
class ImeButtonsView : public views::View {
  METADATA_HEADER(ImeButtonsView, views::View)

 public:
  ImeButtonsView(ImeMenuTray* ime_menu_tray,
                 bool show_emoji,
                 bool show_handwriting,
                 bool show_voice)
      : ime_menu_tray_(ime_menu_tray) {
    DCHECK(ime_menu_tray_);
    SetID(VIEW_ID_IME_BUTTONS_VIEW);
    Init(show_emoji, show_handwriting, show_voice);
  }
  ImeButtonsView(const ImeButtonsView&) = delete;
  ImeButtonsView& operator=(const ImeButtonsView&) = delete;

  ~ImeButtonsView() override = default;

  void KeysetButtonPressed(input_method::ImeKeyset keyset) {
    // TODO(dcheng): When https://crbug.com/742517 is fixed, Mojo will
    // generate a constant for the number of values in the enum. For now, we
    // just define it here and keep it in sync with the enum.
    const int kImeKeysetUmaBoundary = 4;
    UMA_HISTOGRAM_ENUMERATION("InputMethod.ImeMenu.EmojiHandwritingVoiceButton",
                              keyset, kImeKeysetUmaBoundary);

    // The |keyset| will be used for drawing input view keyset in IME
    // extensions. ImeMenuTray::ShowKeyboardWithKeyset() will deal with
    // the |keyset| string to generate the right input view url.
    ime_menu_tray_->ShowKeyboardWithKeyset(keyset);
  }

 private:
  void Init(bool show_emoji, bool show_handwriting, bool show_voice) {
    auto box_layout = std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal);
    box_layout->set_minimum_cross_axis_size(kTrayPopupItemMinHeight);
    SetLayoutManager(std::move(box_layout));
    SetBorder(views::CreateEmptyBorder(
        gfx::Insets::VH(0, kMenuExtraMarginFromLeftEdge)));
    if (show_emoji) {
      emoji_button_ = new SystemMenuButton(
          base::BindRepeating(&ImeButtonsView::KeysetButtonPressed,
                              base::Unretained(this),
                              input_method::ImeKeyset::kEmoji),
          kImeMenuEmoticonIcon, IDS_ASH_STATUS_TRAY_IME_EMOJI);
      emoji_button_->SetID(kEmojiButtonId);
      AddChildView(emoji_button_.get());
    }

    if (show_handwriting) {
      handwriting_button_ = new SystemMenuButton(
          base::BindRepeating(&ImeButtonsView::KeysetButtonPressed,
                              base::Unretained(this),
                              input_method::ImeKeyset::kHandwriting),
          kImeMenuWriteIcon, IDS_ASH_STATUS_TRAY_IME_HANDWRITING);
      AddChildView(handwriting_button_.get());
    }

    if (show_voice) {
      voice_button_ = new SystemMenuButton(
          base::BindRepeating(&ImeButtonsView::KeysetButtonPressed,
                              base::Unretained(this),
                              input_method::ImeKeyset::kVoice),
          kImeMenuMicrophoneIcon, IDS_ASH_STATUS_TRAY_IME_VOICE);
      voice_button_->SetID(kVoiceButtonId);
      AddChildView(voice_button_.get());
    }
  }

  raw_ptr<ImeMenuTray, DanglingUntriaged> ime_menu_tray_;
  raw_ptr<SystemMenuButton> emoji_button_;
  raw_ptr<SystemMenuButton> handwriting_button_;
  raw_ptr<SystemMenuButton> voice_button_;
};

BEGIN_METADATA(ImeButtonsView)
END_METADATA

// A list of available IMEs shown in the opt-in IME menu, which has a
// different height depending on the number of IMEs in the list.
class ImeMenuListView : public ImeListView {
  METADATA_HEADER(ImeMenuListView, ImeListView)

 public:
  ImeMenuListView() : ImeMenuListView(std::make_unique<Delegate>()) {
    SetID(VIEW_ID_IME_MENU_LIST_VIEW);
  }
  ImeMenuListView(const ImeMenuListView&) = delete;
  ImeMenuListView& operator=(const ImeMenuListView&) = delete;

  ~ImeMenuListView() override = default;

 private:
  class Delegate : public DetailedViewDelegate {
   public:
    Delegate() : DetailedViewDelegate(nullptr /* tray_controller */) {}

    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    // DetailedViewDelegate:
    void TransitionToMainView(bool restore_focus) override {}
    void CloseBubble() override {}
    gfx::Insets GetScrollViewMargin() const override {
      return ShouldShowBottomButtons()
                 ? kQsScrollViewMargin
                 : kQsScrollViewMarginWithoutBottomButtons;
    }
  };

  explicit ImeMenuListView(std::unique_ptr<Delegate> delegate)
      : ImeListView(delegate.get()) {
    set_should_focus_ime_after_selection_with_keyboard(true);
    delegate_ = std::move(delegate);
  }

  // ImeListView:
  void Layout(PassKey) override {
    gfx::Range height_range = GetImeListViewRange();
    scroller()->ClipHeightTo(height_range.start(), height_range.end());
    LayoutSuperclass<ImeListView>(this);
  }

  std::unique_ptr<Delegate> delegate_;
};

BEGIN_METADATA(ImeMenuListView)
END_METADATA

}  // namespace

ImeMenuTray::ImeMenuTray(Shelf* shelf)
    : TrayBackgroundView(shelf, TrayBackgroundViewCatalogName::kImeMenu),
      ime_controller_(Shell::Get()->ime_controller()),
      label_(nullptr),
      image_view_(nullptr),
      keyboard_suppressed_(false),
      show_bubble_after_keyboard_hidden_(false),
      is_emoji_enabled_(false),
      is_handwriting_enabled_(false),
      is_voice_enabled_(false) {
  DCHECK(ime_controller_);
  SetCallback(base::BindRepeating(&ImeMenuTray::OnTrayButtonPressed,
                                  weak_ptr_factory_.GetWeakPtr()));
  CreateLabel();
  SystemTrayNotifier* tray_notifier = Shell::Get()->system_tray_notifier();
  tray_notifier->AddIMEObserver(this);
  tray_notifier->AddVirtualKeyboardObserver(this);

  // Show the tray even if virtual keyboard is shown. (Other tray buttons will
  // be hidden).
  set_show_with_virtual_keyboard(true);
}

ImeMenuTray::~ImeMenuTray() {
  if (bubble_) {
    bubble_->bubble_view()->ResetDelegate();
  }
  SystemTrayNotifier* tray_notifier = Shell::Get()->system_tray_notifier();
  tray_notifier->RemoveIMEObserver(this);
  tray_notifier->RemoveVirtualKeyboardObserver(this);
  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  if (keyboard_controller->HasObserver(this)) {
    keyboard_controller->RemoveObserver(this);
  }
}

void ImeMenuTray::OnTrayButtonPressed() {
  UserMetricsRecorder::RecordUserClickOnTray(
      LoginMetricsRecorder::TrayClickTarget::kImeTray);

  if (GetBubbleWidget()) {
    CloseBubble();
    return;
  }

  ShowBubble();
}

void ImeMenuTray::ShowImeMenuBubbleInternal() {
  TrayBubbleView::InitParams init_params = CreateInitParamsForTrayBubble(this);
  if (IsKioskSession()) {
    init_params.insets = kKioskBubbleViewPadding;
  }

  std::unique_ptr<TrayBubbleView> bubble_view =
      std::make_unique<TrayBubbleView>(init_params);

  // Add a title item with a separator on the top of the IME menu.
  bubble_view->AddChildView(std::make_unique<ImeTitleView>());

  // Adds IME list to the bubble.
  ime_list_view_ =
      bubble_view->AddChildView(std::make_unique<ImeMenuListView>());
  ime_list_view_->Init(ShouldShowKeyboardToggle(),
                       ImeListView::SHOW_SINGLE_IME);

  if (ShouldShowBottomButtons()) {
    auto* ime_controller = Shell::Get()->ime_controller();

    is_emoji_enabled_ = ime_controller->is_emoji_enabled();
    is_handwriting_enabled_ = ime_controller->is_handwriting_enabled();
    is_voice_enabled_ = ShouldShowVoiceButton();

    bubble_view->AddChildView(std::make_unique<ImeButtonsView>(
        this, is_emoji_enabled_, is_handwriting_enabled_, is_voice_enabled_));
  } else {
    is_emoji_enabled_ = is_handwriting_enabled_ = is_voice_enabled_ = false;
  }

  bubble_ = std::make_unique<TrayBubbleWrapper>(this);
  bubble_->ShowBubble(std::move(bubble_view));
  SetIsActive(true);

  Shell::Get()->system_tray_notifier()->NotifyImeMenuTrayBubbleShown();
}

void ImeMenuTray::ShowKeyboardWithKeyset(input_method::ImeKeyset keyset) {
  CloseBubble();

  // Show emoji in the same way as other means of opening and showing emoji
  // for laptop and tablet mode.
  if (keyset == input_method::ImeKeyset::kEmoji) {
    ui::ShowEmojiPanel();
  } else {
    Shell::Get()
        ->keyboard_controller()
        ->virtual_keyboard_controller()
        ->ForceShowKeyboardWithKeyset(keyset);
  }
}

bool ImeMenuTray::ShouldShowKeyboardToggle() const {
  return keyboard_suppressed_ && !Shell::Get()
                                      ->accessibility_controller()
                                      ->virtual_keyboard()
                                      .enabled();
}

void ImeMenuTray::OnThemeChanged() {
  TrayBackgroundView::OnThemeChanged();
  UpdateTrayLabel();
}

std::u16string ImeMenuTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(IDS_ASH_IME_MENU_ACCESSIBLE_NAME);
}

void ImeMenuTray::HandleLocaleChange() {
  if (image_view_) {
    image_view_->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_IME));
  }

  if (label_) {
    label_->SetTooltipText(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_IME));
  }
}

void ImeMenuTray::HideBubbleWithView(const TrayBubbleView* bubble_view) {
  if (bubble_->bubble_view() == bubble_view) {
    CloseBubble();
  }
}

void ImeMenuTray::ClickedOutsideBubble(const ui::LocatedEvent& event) {
  CloseBubble();
}

void ImeMenuTray::UpdateTrayItemColor(bool is_active) {
  UpdateTrayImageOrLabelColor(
      extension_ime_util::IsArcIME(ime_controller_->current_ime().id));
}

void ImeMenuTray::CloseBubbleInternal() {
  bubble_.reset();
  ime_list_view_ = nullptr;
  SetIsActive(false);
  shelf()->UpdateAutoHideState();
}

void ImeMenuTray::ShowBubble() {
  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  if (keyboard_controller->IsKeyboardVisible()) {
    show_bubble_after_keyboard_hidden_ = true;
    keyboard_controller->AddObserver(this);
    keyboard_controller->HideKeyboardExplicitlyBySystem();
  } else {
    base::RecordAction(base::UserMetricsAction("Tray_ImeMenu_Opened"));
    ShowImeMenuBubbleInternal();
  }
}

TrayBubbleView* ImeMenuTray::GetBubbleView() {
  return bubble_ ? bubble_->GetBubbleView() : nullptr;
}

views::Widget* ImeMenuTray::GetBubbleWidget() const {
  return bubble_ ? bubble_->GetBubbleWidget() : nullptr;
}

void ImeMenuTray::AddedToWidget() {
  // SetVisiblePreferred cannot be called until after the view has been added to
  // a widget.
  auto* ime_controller = Shell::Get()->ime_controller();

  // On the primary display, `ImeMenuTray` is created for the primary shelf, and
  // then `ImeObserver`s (of which `ImeMenuTray` is one) can react to IME menu
  // activation. If the IME menu is active, and then a display is connected,
  // this object will not have been notified of previous IME menu activations.
  // So check for that here and modify visibility. Only necessary for secondary
  // displays.
  if (!ime_controller || !ime_controller->is_menu_active()) {
    return;
  }

  SetVisiblePreferred(true);
  UpdateTrayLabel();
}

void ImeMenuTray::OnIMERefresh() {
  UpdateTrayLabel();
  if (bubble_ && ime_list_view_) {
    ime_list_view_->Update(
        ime_controller_->current_ime().id, ime_controller_->GetVisibleImes(),
        ime_controller_->current_ime_menu_items(), ShouldShowKeyboardToggle(),
        ImeListView::SHOW_SINGLE_IME);
  }
}

void ImeMenuTray::OnIMEMenuActivationChanged(bool is_activated) {
  SetVisiblePreferred(is_activated);
  if (is_activated) {
    UpdateTrayLabel();
  } else {
    CloseBubble();
  }
}

std::u16string ImeMenuTray::GetAccessibleNameForBubble() {
  return l10n_util::GetStringUTF16(IDS_ASH_IME_MENU_ACCESSIBLE_NAME);
}

bool ImeMenuTray::ShouldEnableExtraKeyboardAccessibility() {
  return Shell::Get()->accessibility_controller()->spoken_feedback().enabled();
}

void ImeMenuTray::HideBubble(const TrayBubbleView* bubble_view) {
  HideBubbleWithView(bubble_view);
}

void ImeMenuTray::OnKeyboardHidden(bool is_temporary_hide) {
  if (show_bubble_after_keyboard_hidden_) {
    show_bubble_after_keyboard_hidden_ = false;
    auto* keyboard_controller = keyboard::KeyboardUIController::Get();
    keyboard_controller->RemoveObserver(this);

    ShowImeMenuBubbleInternal();
    return;
  }
}

void ImeMenuTray::OnKeyboardSuppressionChanged(bool suppressed) {
  if (suppressed != keyboard_suppressed_ && bubble_) {
    CloseBubble();
  }
  keyboard_suppressed_ = suppressed;
}

bool ImeMenuTray::AnyBottomButtonShownForTest() const {
  return is_emoji_enabled_ || is_handwriting_enabled_ || is_voice_enabled_;
}

void ImeMenuTray::UpdateTrayLabel() {
  const ImeInfo& current_ime = ime_controller_->current_ime();

  // For ARC IMEs, we use the globe icon instead of the short name of the active
  // IME.
  if (extension_ime_util::IsArcIME(current_ime.id)) {
    CreateImageView();
    UpdateTrayImageOrLabelColor(/*is_image=*/true);
    return;
  }

  // Updates the tray label based on the current input method.
  CreateLabel();
  UpdateTrayImageOrLabelColor(/*is_image=*/false);

  if (current_ime.third_party) {
    label_->SetText(current_ime.short_name + u"*");
  } else {
    label_->SetText(current_ime.short_name);
  }
}

void ImeMenuTray::CreateLabel() {
  // Do nothing if label_ is already created.
  if (label_) {
    return;
  }
  // Remove image_view_ at first if it's created.
  if (image_view_) {
    tray_container()->RemoveChildView(image_view_);
    image_view_ = nullptr;
  }
  label_ = new ImeMenuLabel();
  SetupLabelForTray(label_);
  label_->SetElideBehavior(gfx::TRUNCATE);
  label_->SetTooltipText(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_IME));
  tray_container()->AddChildView(label_.get());
}

void ImeMenuTray::CreateImageView() {
  // Do nothing if image_view_ is already created.
  if (image_view_) {
    return;
  }
  // Remove label_ at first if it's created.
  if (label_) {
    tray_container()->RemoveChildView(label_);
    label_ = nullptr;
  }
  image_view_ = new ImeMenuImageView();
  image_view_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_IME));
  tray_container()->AddChildView(image_view_.get());
}

void ImeMenuTray::UpdateTrayImageOrLabelColor(bool is_image) {
  const ui::ColorId color_id =
      is_active() ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                  : cros_tokens::kCrosSysOnSurface;

  if (is_image) {
    image_view_->SetImage(
        ui::ImageModel::FromVectorIcon(kShelfGlobeIcon, color_id));
    return;
  }

  label_->SetEnabledColorId(color_id);
}

BEGIN_METADATA(ImeMenuTray)
END_METADATA

}  // namespace ash
