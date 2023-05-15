// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_user_view.h"

#include <memory>

#include "ash/login/ui/animated_rounded_image_view.h"
#include "ash/login/ui/hover_notifier.h"
#include "ash/login/ui/image_parser.h"
#include "ash/login/ui/login_button.h"
#include "ash/login/ui/login_constants.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/login/ui/user_switch_flip_animation.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/session/user_info.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/user_manager/user_type.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/painter.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {

// Vertical spacing between icon, label, and authentication UI.
constexpr int kVerticalSpacingBetweenEntriesDp = 24;
// Horizontal spacing between username label and the dropdown icon.
constexpr int kDistanceBetweenUsernameAndDropdownDp = 8;
// Distance between user icon and the user label in small/extra-small layouts.
constexpr int kSmallManyDistanceFromUserIconToUserLabelDp = 16;

constexpr int kDropdownIconSizeDp = 28;

// Width/height of the user view. Ensures proper centering.
constexpr int kLargeUserViewWidthDp = 306;
constexpr int kLargeUserViewHeightDp = 346;
constexpr int kSmallUserViewWidthDp = 304;
constexpr int kExtraSmallUserViewWidthDp = 282;

// Width/height of the user image.
constexpr int kLargeUserImageSizeDp = 96;
constexpr int kSmallUserImageSizeDp = 74;
constexpr int kExtraSmallUserImageSizeDp = 60;

// Width/height of the enterprise icon circle.
constexpr int kLargeUserIconSizeDp = 26;
constexpr int kSmallUserIconSizeDp = 26;
constexpr int kExtraSmallUserIconSizeDp = 22;

// Size of the icon compared to the one of the white circle.
constexpr float kIconProportion = 0.55f;

// Opacity for when the user view is active/focused and inactive.
constexpr float kOpaqueUserViewOpacity = 1.f;
constexpr float kTransparentUserViewOpacity = 0.63f;
constexpr float kUserFadeAnimationDurationMs = 180;

constexpr char kUserViewClassName[] = "UserView";
constexpr char kLoginUserImageClassName[] = "LoginUserImage";
constexpr char kLoginUserLabelClassName[] = "LoginUserLabel";

// An animation decoder which does not rescale based on the current image_scale.
class PassthroughAnimationDecoder
    : public AnimatedRoundedImageView::AnimationDecoder {
 public:
  explicit PassthroughAnimationDecoder(const AnimationFrames& frames)
      : frames_(frames) {}

  PassthroughAnimationDecoder(const PassthroughAnimationDecoder&) = delete;
  PassthroughAnimationDecoder& operator=(const PassthroughAnimationDecoder&) =
      delete;

  ~PassthroughAnimationDecoder() override = default;

  // AnimatedRoundedImageView::AnimationDecoder:
  AnimationFrames Decode(float image_scale) override { return frames_; }

 private:
  AnimationFrames frames_;
};

class IconRoundedView : public views::View {
 public:
  explicit IconRoundedView(int size) : size_(size) {}
  ~IconRoundedView() override = default;

  IconRoundedView(const IconRoundedView&) = delete;
  IconRoundedView& operator=(const IconRoundedView&) = delete;

  void OnPaint(gfx::Canvas* canvas) override {
    View::OnPaint(canvas);

    const int radius = size_ / 2;
    const gfx::Rect content_bounds(GetContentsBounds());
    const gfx::Point center_circle(content_bounds.width() - radius,
                                   content_bounds.height() - radius);
    const gfx::Point left_corner_icon(
        std::round(content_bounds.width() - radius * (1 + kIconProportion)),
        std::round(content_bounds.height() - radius * (1 + kIconProportion)));
    gfx::Rect image_bounds(left_corner_icon, icon_.size());
    SkPath path;
    path.addRect(gfx::RectToSkRect(image_bounds));
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kIconColorSecondaryBackground));
    flags.setStyle(cc::PaintFlags::kFill_Style);
    // The colored circle on which we paint the icon.
    canvas->DrawCircle(center_circle, radius, flags);
    canvas->DrawImageInPath(icon_, image_bounds.x(), image_bounds.y(), path,
                            flags);
  }

  // views::View:
  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    icon_ = gfx::ImageSkiaOperations::CreateResizedImage(
        gfx::CreateVectorIcon(
            chromeos::kEnterpriseIcon,
            AshColorProvider::Get()->GetContentLayerColor(
                AshColorProvider::ContentLayerType::kIconColorSecondary)),
        skia::ImageOperations::RESIZE_BEST,
        gfx::Size(size_ * kIconProportion, size_ * kIconProportion));
    SchedulePaint();
  }

 private:
  const int size_;
  gfx::ImageSkia icon_;
};

}  // namespace

// Renders a user's profile icon.
class LoginUserView::UserImage : public NonAccessibleView {
 public:
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(LoginUserView::UserImage* view) : view_(view) {}
    ~TestApi() = default;

    views::View* enterprise_icon() const { return view_->enterprise_icon_; }

   private:
    const raw_ptr<LoginUserView::UserImage, ExperimentalAsh> view_;
  };

  explicit UserImage(LoginDisplayStyle style)
      : NonAccessibleView(kLoginUserImageClassName) {
    SetLayoutManager(std::make_unique<views::FillLayout>());

    const int image_size = GetImageSize(style);
    image_ = new AnimatedRoundedImageView(gfx::Size(image_size, image_size),
                                          image_size / 2);
    AddChildView(image_.get());

    const int icon_size = GetIconSize(style);
    enterprise_icon_ = new IconRoundedView(icon_size);
    enterprise_icon_->SetVisible(false);
    AddChildView(enterprise_icon_.get());
  }

  UserImage(const UserImage&) = delete;
  UserImage& operator=(const UserImage&) = delete;

  ~UserImage() override = default;

  void UpdateForUser(const LoginUserInfo& user) {
    // Set the initial image from |avatar| since we already have it available.
    // Then, decode the bytes via blink's PNG decoder and play any animated
    // frames if they are available.
    if (!user.basic_user_info.avatar.image.isNull()) {
      image_->SetImage(user.basic_user_info.avatar.image);
    }

    // Decode the avatar using blink, as blink's PNG decoder supports APNG,
    // which is the format used for the animated avators.
    if (!user.basic_user_info.avatar.bytes.empty()) {
      DecodeAnimation(user.basic_user_info.avatar.bytes,
                      base::BindOnce(&LoginUserView::UserImage::OnImageDecoded,
                                     weak_factory_.GetWeakPtr()));
    }

    bool is_managed =
        user.user_account_manager ||
        user.basic_user_info.type == user_manager::USER_TYPE_PUBLIC_ACCOUNT;
    enterprise_icon_->SetVisible(is_managed);
  }

  void SetAnimationEnabled(bool enable) {
    animation_enabled_ = enable;
    image_->SetAnimationPlayback(
        animation_enabled_
            ? AnimatedRoundedImageView::Playback::kRepeat
            : AnimatedRoundedImageView::Playback::kFirstFrameOnly);
  }

 private:
  void OnImageDecoded(AnimationFrames animation) {
    // If there is only a single frame to display, show the existing avatar.
    if (animation.size() <= 1) {
      LOG_IF(ERROR, animation.empty()) << "Decoding user avatar failed";
      return;
    }

    image_->SetAnimationDecoder(
        std::make_unique<PassthroughAnimationDecoder>(animation),
        animation_enabled_
            ? AnimatedRoundedImageView::Playback::kRepeat
            : AnimatedRoundedImageView::Playback::kFirstFrameOnly);
  }

  static int GetImageSize(LoginDisplayStyle style) {
    switch (style) {
      case LoginDisplayStyle::kLarge:
        return kLargeUserImageSizeDp;
      case LoginDisplayStyle::kSmall:
        return kSmallUserImageSizeDp;
      case LoginDisplayStyle::kExtraSmall:
        return kExtraSmallUserImageSizeDp;
    }
  }

  static int GetIconSize(LoginDisplayStyle style) {
    switch (style) {
      case LoginDisplayStyle::kLarge:
        return kLargeUserIconSizeDp;
      case LoginDisplayStyle::kSmall:
        return kSmallUserIconSizeDp;
      case LoginDisplayStyle::kExtraSmall:
        return kExtraSmallUserIconSizeDp;
    }
  }

  raw_ptr<AnimatedRoundedImageView, ExperimentalAsh> image_ = nullptr;
  raw_ptr<IconRoundedView, ExperimentalAsh> enterprise_icon_ = nullptr;
  bool animation_enabled_ = false;

  base::WeakPtrFactory<UserImage> weak_factory_{this};
};

// Shows the user's name.
class LoginUserView::UserLabel : public NonAccessibleView {
 public:
  UserLabel(LoginDisplayStyle style, int label_width)
      : NonAccessibleView(kLoginUserLabelClassName), label_width_(label_width) {
    SetLayoutManager(std::make_unique<views::FillLayout>());

    user_name_ = new views::Label();
    user_name_->SetSubpixelRenderingEnabled(false);
    user_name_->SetAutoColorReadabilityEnabled(false);
    user_name_->SetEnabledColorId(kColorAshTextColorPrimary);

    const gfx::FontList& base_font_list = views::Label::GetDefaultFontList();
    const gfx::FontList font_list(
        {login_views_utils::kGoogleSansFont}, base_font_list.GetFontStyle(),
        base_font_list.GetFontSize(), base_font_list.GetFontWeight());

    switch (style) {
      case LoginDisplayStyle::kLarge:
        user_name_->SetFontList(font_list.Derive(
            12, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::NORMAL));
        break;
      case LoginDisplayStyle::kSmall:
        user_name_->SetFontList(font_list.Derive(
            8, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::NORMAL));
        break;
      case LoginDisplayStyle::kExtraSmall:
        // TODO(jdufault): match font against spec.
        user_name_->SetFontList(font_list.Derive(
            6, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::NORMAL));
        break;
    }

    AddChildView(user_name_.get());
  }

  UserLabel(const UserLabel&) = delete;
  UserLabel& operator=(const UserLabel&) = delete;

  ~UserLabel() override = default;

  void UpdateForUser(const LoginUserInfo& user) {
    std::string display_name = user.basic_user_info.display_name;
    // display_name can be empty in debug builds with stub users.
    if (display_name.empty()) {
      display_name = user.basic_user_info.display_email;
    }

    user_name_->SetText(gfx::ElideText(base::UTF8ToUTF16(display_name),
                                       user_name_->font_list(), label_width_,
                                       gfx::ElideBehavior::ELIDE_TAIL));
  }

  const std::u16string& displayed_name() const { return user_name_->GetText(); }

 private:
  raw_ptr<views::Label, ExperimentalAsh> user_name_ = nullptr;
  const int label_width_;
};

// A button embedded inside of LoginUserView, which is activated whenever the
// user taps anywhere in the LoginUserView. Previously, LoginUserView was a
// views::Button, but this breaks ChromeVox as it does not expect buttons to
// have any children (ie, the dropdown button).
class LoginUserView::TapButton : public views::Button {
 public:
  TapButton(PressedCallback callback, LoginUserView* parent)
      : views::Button(std::move(callback)), parent_(parent) {}

  TapButton(const TapButton&) = delete;
  TapButton& operator=(const TapButton&) = delete;

  ~TapButton() override = default;

  // views::Button:
  void OnFocus() override {
    views::Button::OnFocus();
    parent_->UpdateOpacity();
  }
  void OnBlur() override {
    views::Button::OnBlur();
    parent_->UpdateOpacity();
  }
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    // TODO(https://crbug.com/1065516): Define the button name.
    node_data->SetNameExplicitlyEmpty();
    Button::GetAccessibleNodeData(node_data);
  }

 private:
  const raw_ptr<LoginUserView, ExperimentalAsh> parent_;
};

// LoginUserView is defined after LoginUserView::UserLabel so it can access the
// class members.

LoginUserView::TestApi::TestApi(LoginUserView* view) : view_(view) {}

LoginUserView::TestApi::~TestApi() = default;

LoginDisplayStyle LoginUserView::TestApi::display_style() const {
  return view_->display_style_;
}

const std::u16string& LoginUserView::TestApi::displayed_name() const {
  return view_->user_label_->displayed_name();
}

views::View* LoginUserView::TestApi::user_label() const {
  return view_->user_label_;
}

views::View* LoginUserView::TestApi::tap_button() const {
  return view_->tap_button_;
}

views::View* LoginUserView::TestApi::dropdown() const {
  return view_->dropdown_;
}

LoginRemoveAccountDialog* LoginUserView::TestApi::remove_account_dialog()
    const {
  return view_->remove_account_dialog_;
}

views::View* LoginUserView::TestApi::enterprise_icon() const {
  return LoginUserView::UserImage::TestApi(view_->user_image_)
      .enterprise_icon();
}

void LoginUserView::TestApi::OnTap() const {
  view_->on_tap_.Run();
}

bool LoginUserView::TestApi::is_opaque() const {
  return view_->is_opaque_;
}

// static
int LoginUserView::WidthForLayoutStyle(LoginDisplayStyle style) {
  switch (style) {
    case LoginDisplayStyle::kLarge:
      return kLargeUserViewWidthDp;
    case LoginDisplayStyle::kSmall:
      return kSmallUserViewWidthDp;
    case LoginDisplayStyle::kExtraSmall:
      return kExtraSmallUserViewWidthDp;
  }
}

LoginUserView::LoginUserView(
    LoginDisplayStyle style,
    bool show_dropdown,
    const OnTap& on_tap,
    const OnRemoveWarningShown& on_remove_warning_shown,
    const OnRemove& on_remove)
    : on_tap_(on_tap),
      on_remove_warning_shown_(on_remove_warning_shown),
      on_remove_(on_remove),
      display_style_(style) {
  // show_dropdown can only be true when the user view is rendering in large
  // mode.
  DCHECK(!show_dropdown || style == LoginDisplayStyle::kLarge);
  // |on_remove_warning_shown| and |on_remove| is only available iff
  // |show_dropdown| is true.
  DCHECK(show_dropdown == !!on_remove_warning_shown);
  DCHECK(show_dropdown == !!on_remove);

  user_image_ = new UserImage(style);
  int label_width =
      WidthForLayoutStyle(style) -
      2 * (kDistanceBetweenUsernameAndDropdownDp + kDropdownIconSizeDp);
  user_label_ = new UserLabel(style, label_width);
  if (show_dropdown) {
    dropdown_ = new LoginButton(base::BindRepeating(
        &LoginUserView::DropdownButtonPressed, base::Unretained(this)));
    dropdown_->SetHasInkDropActionOnClick(false);
    dropdown_->SetPreferredSize(
        gfx::Size(kDropdownIconSizeDp, kDropdownIconSizeDp));
    dropdown_->SetFocusBehavior(FocusBehavior::ALWAYS);
    dropdown_->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(kLockScreenDropdownIcon,
                                       kColorAshIconColorPrimary));
  }
  tap_button_ = new TapButton(on_tap_, this);
  SetTapEnabled(true);

  switch (style) {
    case LoginDisplayStyle::kLarge:
      SetLargeLayout();
      break;
    case LoginDisplayStyle::kSmall:
    case LoginDisplayStyle::kExtraSmall:
      SetSmallishLayout();
      break;
  }

  // Layer rendering is needed for animation. We apply animations to child views
  // separately to reduce overdraw.
  auto setup_layer = [](views::View* view) {
    view->SetPaintToLayer();
    view->layer()->SetFillsBoundsOpaquely(false);
    view->layer()->SetOpacity(kTransparentUserViewOpacity);
    view->layer()->GetAnimator()->set_preemption_strategy(
        ui::LayerAnimator::PreemptionStrategy::
            IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  };
  setup_layer(user_image_);
  setup_layer(user_label_);
  if (dropdown_) {
    setup_layer(dropdown_);
  }

  hover_notifier_ = std::make_unique<HoverNotifier>(
      this,
      base::BindRepeating(&LoginUserView::OnHover, base::Unretained(this)));

  if (ash::Shell::HasInstance()) {
    display_observation_.Observe(ash::Shell::Get()->display_configurator());
  }
}

LoginUserView::~LoginUserView() {
  DeleteDialog();
}

void LoginUserView::UpdateForUser(const LoginUserInfo& user, bool animate) {
  current_user_ = user;

  DeleteDialog();

  remove_account_dialog_ = new LoginRemoveAccountDialog(
      current_user_,
      dropdown_ != nullptr ? dropdown_->AsWeakPtr() : nullptr /*anchor_view*/,
      dropdown_ /*bubble_opener*/, on_remove_warning_shown_, on_remove_);
  remove_account_dialog_->SetVisible(false);

  if (animate) {
    // Stop any existing animation.
    user_image_->layer()->GetAnimator()->StopAnimating();

    // Create the image flip animation.
    auto image_transition = std::make_unique<UserSwitchFlipAnimation>(
        user_image_->width(), 0 /*start_degrees*/, 90 /*midpoint_degrees*/,
        180 /*end_degrees*/,
        base::Milliseconds(login::kChangeUserAnimationDurationMs),
        gfx::Tween::Type::EASE_OUT,
        base::BindOnce(&LoginUserView::UpdateCurrentUserState,
                       base::Unretained(this)));
    auto* image_sequence =
        new ui::LayerAnimationSequence(std::move(image_transition));
    user_image_->layer()->GetAnimator()->StartAnimation(image_sequence);

    // Create opacity fade animation, which applies to the entire element.
    bool is_opaque = this->is_opaque_;
    auto make_opacity_sequence = [is_opaque]() {
      auto make_opacity_element = [](float target_opacity) {
        auto element = ui::LayerAnimationElement::CreateOpacityElement(
            target_opacity,
            base::Milliseconds(login::kChangeUserAnimationDurationMs / 2.0f));
        element->set_tween_type(gfx::Tween::Type::EASE_OUT);
        return element;
      };

      auto* opacity_sequence = new ui::LayerAnimationSequence();
      opacity_sequence->AddElement(make_opacity_element(0 /*target_opacity*/));
      opacity_sequence->AddElement(make_opacity_element(
          is_opaque ? kOpaqueUserViewOpacity
                    : kTransparentUserViewOpacity /*target_opacity*/));
      return opacity_sequence;
    };
    user_image_->layer()->GetAnimator()->StartAnimation(
        make_opacity_sequence());
    user_label_->layer()->GetAnimator()->StartAnimation(
        make_opacity_sequence());
    if (dropdown_) {
      dropdown_->layer()->GetAnimator()->StartAnimation(
          make_opacity_sequence());
    }
  } else {
    // Do not animate, so directly update to the current user.
    UpdateCurrentUserState();
  }
}

void LoginUserView::SetForceOpaque(bool force_opaque) {
  force_opaque_ = force_opaque;
  UpdateOpacity();
}

void LoginUserView::SetTapEnabled(bool enabled) {
  tap_button_->SetFocusBehavior(enabled ? FocusBehavior::ALWAYS
                                        : FocusBehavior::NEVER);
}

void LoginUserView::OnPowerStateChanged(
    chromeos::DisplayPowerState power_state) {
  bool is_display_on = power_state != chromeos::DISPLAY_POWER_ALL_OFF;
  user_image_->SetAnimationEnabled(is_display_on && is_opaque_);
}

const char* LoginUserView::GetClassName() const {
  return kUserViewClassName;
}

gfx::Size LoginUserView::CalculatePreferredSize() const {
  switch (display_style_) {
    case LoginDisplayStyle::kLarge:
      return gfx::Size(kLargeUserViewWidthDp, kLargeUserViewHeightDp);
    case LoginDisplayStyle::kSmall:
      return gfx::Size(kSmallUserViewWidthDp, kSmallUserImageSizeDp);
    case LoginDisplayStyle::kExtraSmall:
      return gfx::Size(kExtraSmallUserViewWidthDp, kExtraSmallUserImageSizeDp);
  }
}

void LoginUserView::Layout() {
  views::View::Layout();
  tap_button_->SetBoundsRect(GetLocalBounds());
}

void LoginUserView::RequestFocus() {
  tap_button_->RequestFocus();
}

views::View::Views LoginUserView::GetChildrenInZOrder() {
  auto children = views::View::GetChildrenInZOrder();
  const auto move_child_to_top = [&](View* child) {
    auto it = base::ranges::find(children, child);
    DCHECK(it != children.end());
    std::rotate(it, it + 1, children.end());
  };
  move_child_to_top(tap_button_);
  if (dropdown_) {
    move_child_to_top(dropdown_);
  }
  return children;
}

void LoginUserView::OnHover(bool has_hover) {
  UpdateOpacity();
}

void LoginUserView::DropdownButtonPressed() {
  DCHECK(dropdown_);
  DCHECK(remove_account_dialog_);

  // If the remove account dialog is showing, just close it.
  if (remove_account_dialog_->GetVisible()) {
    remove_account_dialog_->Hide();
    return;
  }

  bool opener_focused = remove_account_dialog_->GetBubbleOpener() &&
                        remove_account_dialog_->GetBubbleOpener()->HasFocus();

  if (!remove_account_dialog_->parent()) {
    login_views_utils::GetBubbleContainer(this)->AddChildView(
        remove_account_dialog_.get());
  }

  // Reset state in case the remove-user button was clicked once previously.
  remove_account_dialog_->ResetState();
  remove_account_dialog_->Show();

  // If the remove account dialog was opened by pressing Enter on the focused
  // dropdown, focus should automatically go to the remove-user button (for
  // keyboard accessibility).
  if (opener_focused) {
    remove_account_dialog_->RequestFocus();
  }
}

void LoginUserView::UpdateCurrentUserState() {
  std::u16string accessible_name;
  auto email = base::UTF8ToUTF16(current_user_.basic_user_info.display_email);
  if (current_user_.user_account_manager) {
    accessible_name = l10n_util::GetStringFUTF16(
        IDS_ASH_LOGIN_POD_MANAGED_ACCESSIBLE_NAME, email);
  } else if (current_user_.basic_user_info.type ==
             user_manager::USER_TYPE_PUBLIC_ACCOUNT) {
    accessible_name = l10n_util::GetStringFUTF16(
        IDS_ASH_LOGIN_POD_MANAGED_ACCESSIBLE_NAME,
        base::UTF8ToUTF16(current_user_.basic_user_info.display_name));
  } else {
    accessible_name = email;
  }
  tap_button_->SetAccessibleName(accessible_name);
  if (dropdown_) {
    // The accessible name for the dropdown depends on whether it also contains
    // the remove user button for the user in question.
    accessible_name = l10n_util::GetStringFUTF16(
        current_user_.can_remove
            ? IDS_ASH_LOGIN_POD_REMOVE_ACCOUNT_DIALOG_BUTTON_ACCESSIBLE_NAME
            : IDS_ASH_LOGIN_POD_ACCOUNT_DIALOG_BUTTON_ACCESSIBLE_NAME,
        email);
    dropdown_->SetAccessibleName(accessible_name);
  }

  user_image_->UpdateForUser(current_user_);
  user_label_->UpdateForUser(current_user_);
  Layout();
}

void LoginUserView::UpdateOpacity() {
  bool was_opaque = is_opaque_;
  is_opaque_ =
      force_opaque_ || tap_button_->IsMouseHovered() || tap_button_->HasFocus();
  if (was_opaque == is_opaque_) {
    return;
  }

  // Animate to new opacity.
  auto build_settings = [](views::View* view)
      -> std::unique_ptr<ui::ScopedLayerAnimationSettings> {
    auto settings = std::make_unique<ui::ScopedLayerAnimationSettings>(
        view->layer()->GetAnimator());
    settings->SetTransitionDuration(
        base::Milliseconds(kUserFadeAnimationDurationMs));
    settings->SetTweenType(gfx::Tween::Type::EASE_IN_OUT);
    return settings;
  };
  std::unique_ptr<ui::ScopedLayerAnimationSettings> user_image_settings =
      build_settings(user_image_);
  std::unique_ptr<ui::ScopedLayerAnimationSettings> user_label_settings =
      build_settings(user_label_);
  float target_opacity =
      is_opaque_ ? kOpaqueUserViewOpacity : kTransparentUserViewOpacity;
  user_image_->layer()->SetOpacity(target_opacity);
  user_label_->layer()->SetOpacity(target_opacity);
  if (dropdown_) {
    std::unique_ptr<ui::ScopedLayerAnimationSettings> dropdown_settings =
        build_settings(dropdown_);
    dropdown_->layer()->SetOpacity(target_opacity);
  }

  // Animate avatar only if we are opaque.
  user_image_->SetAnimationEnabled(is_opaque_);
}

void LoginUserView::SetLargeLayout() {
  auto* layout = SetLayoutManager(std::make_unique<views::TableLayout>());
  layout
      ->AddColumn(views::LayoutAlignment::kEnd, views::LayoutAlignment::kCenter,
                  1.0f, views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize,
                        kDistanceBetweenUsernameAndDropdownDp)
      .AddColumn(views::LayoutAlignment::kCenter,
                 views::LayoutAlignment::kCenter,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize,
                        kDistanceBetweenUsernameAndDropdownDp)
      .AddColumn(views::LayoutAlignment::kStart,
                 views::LayoutAlignment::kCenter, 1.0f,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddRows(1, views::TableLayout::kFixedSize)
      .AddPaddingRow(views::TableLayout::kFixedSize,
                     kVerticalSpacingBetweenEntriesDp)
      .AddRows(1, views::TableLayout::kFixedSize);

  AddChildView(tap_button_.get());
  layout->SetChildViewIgnoredByLayout(tap_button_, true);

  AddChildView(user_image_.get());
  user_image_->SetProperty(views::kTableColAndRowSpanKey, gfx::Size(5, 1));
  user_image_->SetProperty(views::kTableHorizAlignKey,
                           views::LayoutAlignment::kCenter);

  auto* skip_column = AddChildView(std::make_unique<NonAccessibleView>());
  if (dropdown_) {
    skip_column->SetPreferredSize(dropdown_->GetPreferredSize());
  }

  AddChildView(user_label_.get());

  if (dropdown_) {
    AddChildView(dropdown_.get());
  }
}

void LoginUserView::SetSmallishLayout() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      kSmallManyDistanceFromUserIconToUserLabelDp));
  AddChildView(tap_button_.get());
  tap_button_->SetProperty(views::kViewIgnoredByLayoutKey, true);

  AddChildView(user_image_.get());
  AddChildView(user_label_.get());
}

void LoginUserView::DeleteDialog() {
  if (remove_account_dialog_) {
    if (remove_account_dialog_->parent()) {
      remove_account_dialog_->parent()->RemoveChildView(remove_account_dialog_);
    }
    delete remove_account_dialog_;
    remove_account_dialog_ = nullptr;
  }
}

}  // namespace ash
