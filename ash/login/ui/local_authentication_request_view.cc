// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/local_authentication_request_view.h"

#include <string>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/login/ui/arrow_button_view.h"
#include "ash/login/ui/local_authentication_request_widget.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/public/cpp/login/local_authentication_request_controller.h"
#include "ash/public/cpp/login/login_utils.h"
#include "ash/public/cpp/session/user_info.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/icon_button.h"
#include "ash/style/system_shadow.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/functional/bind.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/cryptohome/common_types.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/session_auth_factors.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/account_id/account_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/vector_icons.h"

namespace ash {

namespace {

// TODO(b/304754895): move the pin request view shared constants to
// ash/login/ui/login_constants.h
constexpr int kLocalAuthenticationRequestViewVerticalInsetDp = 8;
// Minimum inset (= back button inset).
constexpr int kLocalAuthenticationRequestViewHorizontalInsetDp = 8;
constexpr int kLocalAuthenticationRequestViewRoundedCornerRadiusDp = 8;
constexpr int kLocalAuthenticationRequestViewWidthDp = 340;
constexpr int kLocalAuthenticationRequestViewHeightDp = 300;

constexpr int kIconToTitleDistanceDp = 24;
constexpr int kTitleToDescriptionDistanceDp = 8;
constexpr int kDescriptionToAccessCodeDistanceDp = 32;
constexpr int kSubmitButtonBottomMarginDp = 28;

constexpr int kTitleFontSizeDeltaDp = 4;
constexpr int kTitleLineWidthDp = 268;
constexpr int kTitleLineHeightDp = 24;
constexpr int kTitleMaxLines = 4;
constexpr int kDescriptionFontSizeDeltaDp = 0;
constexpr int kDescriptionLineWidthDp = 268;
constexpr int kDescriptionTextLineHeightDp = 18;
constexpr int kDescriptionMaxLines = 4;

constexpr int kAvatarSizeDp = 36;

constexpr int kCrossSizeDp = 20;
constexpr int kBackButtonSizeDp = 36;
constexpr int kLockIconSizeDp = 24;
constexpr int kBackButtonLockIconVerticalOverlapDp = 8;
constexpr int kHeaderHeightDp =
    kBackButtonSizeDp + kLockIconSizeDp - kBackButtonLockIconVerticalOverlapDp;

}  // namespace

LocalAuthenticationRequestView::TestApi::TestApi(
    LocalAuthenticationRequestView* view)
    : view_(view) {
  CHECK(view_);
}

LocalAuthenticationRequestView::TestApi::~TestApi() {
  view_ = nullptr;
}

void LocalAuthenticationRequestView::TestApi::Close() {
  view_->OnClose();
}

void LocalAuthenticationRequestView::TestApi::SubmitPassword(
    const std::string& password) {
  LoginPasswordView::TestApi login_password_view_test_api(
      login_password_view());
  login_password_view_test_api.SubmitPassword(password);
}

LoginButton* LocalAuthenticationRequestView::TestApi::close_button() {
  return view_->close_button_;
}

views::Label* LocalAuthenticationRequestView::TestApi::title_label() {
  return view_->title_label_;
}

views::Label* LocalAuthenticationRequestView::TestApi::description_label() {
  return view_->description_label_;
}

LoginPasswordView*
LocalAuthenticationRequestView::TestApi::login_password_view() {
  return view_->login_password_view_;
}

views::Textfield* LocalAuthenticationRequestView::TestApi::GetInputTextfield()
    const {
  return LoginPasswordView::TestApi(view_->login_password_view_).textfield();
}

LocalAuthenticationRequestViewState
LocalAuthenticationRequestView::TestApi::state() const {
  return view_->state_;
}

LocalAuthenticationRequestView::LocalAuthenticationRequestView(
    LocalAuthenticationCallback local_authentication_callback,
    const std::u16string& title,
    const std::u16string& description,
    base::WeakPtr<Delegate> delegate,
    std::unique_ptr<UserContext> user_context)
    : local_authentication_callback_(std::move(local_authentication_callback)),
      delegate_(delegate),
      default_title_(title),
      default_description_(description),
      auth_performer_(UserDataAuthClient::Get()),
      user_context_(std::move(user_context)) {
  //  ModalType::kSystem is used to get a semi-transparent background behind the
  //  local authentication request view, when it is used directly on a widget.
  //  The overlay consumes all the inputs from the user, so that they can only
  //  interact with the local authentication request view while it is visible.
  SetModalType(ui::mojom::ModalType::kSystem);

  // Main view contains all other views aligned vertically and centered.
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(kLocalAuthenticationRequestViewVerticalInsetDp,
                      kLocalAuthenticationRequestViewHorizontalInsetDp),
      0);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  SetLayoutManager(std::move(layout));

  // Set Backgground color and shape.
  SetPaintToLayer();
  layer()->SetBackgroundBlur(ShelfConfig::Get()->shelf_blur_radius());
  ui::ColorId background_color_id = cros_tokens::kCrosSysSystemBaseElevated;
  SetBackground(views::CreateThemedRoundedRectBackground(
      background_color_id,
      kLocalAuthenticationRequestViewRoundedCornerRadiusDp));

  // Set Border and shadow.
  SetBorder(std::make_unique<views::HighlightBorder>(
      kLocalAuthenticationRequestViewRoundedCornerRadiusDp,
      views::HighlightBorder::Type::kHighlightBorder1));
  shadow_ = SystemShadow::CreateShadowOnNinePatchLayerForView(
      this, SystemShadow::Type::kElevation12);
  shadow_->SetRoundedCornerRadius(
      kLocalAuthenticationRequestViewRoundedCornerRadiusDp);

  // Header view which contains the back button that is aligned top right and
  // the lock icon which is in the bottom center.
  auto header_layout = std::make_unique<views::FillLayout>();
  auto* header = new NonAccessibleView();
  header->SetLayoutManager(std::move(header_layout));
  AddChildView(header);
  auto* header_spacer = new NonAccessibleView();
  header_spacer->SetPreferredSize(gfx::Size(0, kHeaderHeightDp));
  header->AddChildView(header_spacer);

  // Main view user avatar.
  auto* icon_view = new NonAccessibleView();
  icon_view->SetPreferredSize(gfx::Size(0, kHeaderHeightDp));
  auto icon_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0);
  icon_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);
  icon_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  icon_view->SetLayoutManager(std::move(icon_layout));
  header->AddChildView(icon_view);

  auto* avatar_view =
      icon_view->AddChildView(std::make_unique<AnimatedRoundedImageView>(
          gfx::Size(kAvatarSizeDp, kAvatarSizeDp),
          kAvatarSizeDp / 2 /*corner_radius*/));

  const UserAvatar avatar =
      BuildAshUserAvatarForAccountId(user_context_->GetAccountId());

  avatar_view->SetImage(avatar.image);

  // Close button. Note that it should be the last view added to |header| in
  // order to be clickable.
  auto* close_button_view = new NonAccessibleView();
  close_button_view->SetPreferredSize(
      gfx::Size(kLocalAuthenticationRequestViewWidthDp -
                    2 * kLocalAuthenticationRequestViewHorizontalInsetDp,
                kHeaderHeightDp));
  auto close_button_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 0);
  close_button_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);
  close_button_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  close_button_view->SetLayoutManager(std::move(close_button_layout));
  header->AddChildView(close_button_view);

  close_button_ = new LoginButton(base::BindRepeating(
      &LocalAuthenticationRequestView::OnClose, base::Unretained(this)));
  close_button_->SetPreferredSize(
      gfx::Size(kBackButtonSizeDp, kBackButtonSizeDp));
  const ui::ColorId icon_color_id = cros_tokens::kCrosSysOnSurface;
  close_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(views::kIcCloseIcon, icon_color_id,
                                     kCrossSizeDp));
  close_button_->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  close_button_->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  close_button_->GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
      IDS_ASH_LOGIN_LOCAL_AUTHENTICATION_CLOSE_DIALOG_BUTTON));
  close_button_->SetFocusBehavior(FocusBehavior::ALWAYS);
  close_button_view->AddChildView(close_button_.get());

  auto add_spacer = [&](int height) {
    auto* spacer = new NonAccessibleView();
    spacer->SetPreferredSize(gfx::Size(0, height));
    AddChildView(spacer);
  };

  add_spacer(kIconToTitleDistanceDp);

  auto decorate_label = [](views::Label* label) {
    label->SetSubpixelRenderingEnabled(false);
    label->SetAutoColorReadabilityEnabled(false);

    label->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
    label->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  };

  // Main view title.
  title_label_ = new views::Label(default_title_, views::style::CONTEXT_LABEL,
                                  views::style::STYLE_PRIMARY);
  title_label_->SetMultiLine(true);
  title_label_->SetMaxLines(kTitleMaxLines);
  title_label_->SizeToFit(kTitleLineWidthDp);
  title_label_->SetLineHeight(kTitleLineHeightDp);
  title_label_->SetFontList(gfx::FontList().Derive(
      kTitleFontSizeDeltaDp, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
  decorate_label(title_label_);
  AddChildView(title_label_.get());

  add_spacer(kTitleToDescriptionDistanceDp);

  // Main view description.
  description_label_ =
      new views::Label(default_description_, views::style::CONTEXT_LABEL,
                       views::style::STYLE_PRIMARY);
  description_label_->SetMultiLine(true);
  description_label_->SetMaxLines(kDescriptionMaxLines);
  description_label_->SizeToFit(kDescriptionLineWidthDp);
  description_label_->SetLineHeight(kDescriptionTextLineHeightDp);
  description_label_->SetFontList(
      gfx::FontList().Derive(kDescriptionFontSizeDeltaDp, gfx::Font::NORMAL,
                             gfx::Font::Weight::NORMAL));
  decorate_label(description_label_);
  AddChildView(description_label_.get());

  add_spacer(kDescriptionToAccessCodeDistanceDp);

  login_password_view_ = AddChildView(std::make_unique<LoginPasswordView>());

  login_password_view_->SetPaintToLayer();
  login_password_view_->layer()->SetFillsBoundsOpaquely(false);
  login_password_view_->SetDisplayPasswordButtonVisible(true);
  login_password_view_->SetFocusEnabledForTextfield(true);

  login_password_view_->SetPlaceholderText(
      l10n_util::GetStringUTF16(IDS_ASH_IN_SESSION_AUTH_PASSWORD_PLACEHOLDER));
  login_password_view_->Init(
      base::BindRepeating(&LocalAuthenticationRequestView::OnAuthSubmit,
                          base::Unretained(this),
                          /*authenticated_by_pin=*/false),
      base::BindRepeating(&LocalAuthenticationRequestView::OnInputTextChanged,
                          base::Unretained(this)));

  add_spacer(kSubmitButtonBottomMarginDp);

  SetPreferredSize(GetLocalAuthenticationRequestViewSize());

  GetViewAccessibility().SetRole(ax::mojom::Role::kDialog);
  UpdateAccessibleName();
  description_label_changed_subscription_ =
      description_label_->AddTextChangedCallback(base::BindRepeating(
          &LocalAuthenticationRequestView::OnDescriptionLabelTextChanged,
          weak_ptr_factory_.GetWeakPtr()));
}

LocalAuthenticationRequestView::~LocalAuthenticationRequestView() = default;

void LocalAuthenticationRequestView::RequestFocus() {
  login_password_view_->RequestFocus();
}

void LocalAuthenticationRequestView::SetInputEnabled(bool input_enabled) {
  login_password_view_->SetReadOnly(!input_enabled);
}

void LocalAuthenticationRequestView::ClearInput() {
  login_password_view_->Reset();
}

void LocalAuthenticationRequestView::UpdateState(
    LocalAuthenticationRequestViewState state,
    const std::u16string& title,
    const std::u16string& description) {
  state_ = state;
  ui::ColorId color_id = state == LocalAuthenticationRequestViewState::kNormal
                             ? cros_tokens::kCrosSysSystemBaseElevated
                             : cros_tokens::kCrosSysError;
  title_label_->SetText(title);
  description_label_->SetText(description);
  description_label_->SetEnabledColorId(color_id);
}

gfx::Size LocalAuthenticationRequestView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return GetLocalAuthenticationRequestViewSize();
}

views::View* LocalAuthenticationRequestView::GetInitiallyFocusedView() {
  return login_password_view_;
}

std::u16string LocalAuthenticationRequestView::GetAccessibleWindowTitle()
    const {
  return default_title_;
}

void LocalAuthenticationRequestView::OnClose() {
  delegate_->OnClose();
  auth_performer_.InvalidateCurrentAttempts();
  if (LocalAuthenticationRequestWidget::Get()) {
    LocalAuthenticationRequestWidget::Get()->Close(false /* success */,
                                                   std::move(user_context_));
  }
}

void LocalAuthenticationRequestView::UpdatePreferredSize() {
  SetPreferredSize(CalculatePreferredSize({}));
  if (GetWidget()) {
    GetWidget()->CenterWindow(GetPreferredSize());
  }
}

gfx::Size
LocalAuthenticationRequestView::GetLocalAuthenticationRequestViewSize() const {
  return gfx::Size(kLocalAuthenticationRequestViewWidthDp,
                   kLocalAuthenticationRequestViewHeightDp);
}

void LocalAuthenticationRequestView::OnAuthSubmit(
    bool authenticated_by_pin,
    const std::u16string& password) {
  CHECK(!authenticated_by_pin);
  SetInputEnabled(false);

  const auto& auth_factors = user_context_->GetAuthFactorsData();
  const cryptohome::AuthFactor* local_password_factor =
      auth_factors.FindLocalPasswordFactor();
  CHECK_NE(local_password_factor, nullptr);

  const cryptohome::KeyLabel& key_label = local_password_factor->ref().label();

  // Create a copy of `user_context_` so that we don't lose it to std::move
  // for future auth attempts
  auth_performer_.AuthenticateWithPassword(
      key_label.value(), base::UTF16ToUTF8(password), std::move(user_context_),
      base::BindOnce(&LocalAuthenticationRequestView::OnAuthComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void LocalAuthenticationRequestView::OnAuthComplete(
    std::unique_ptr<UserContext> user_context,
    std::optional<AuthenticationError> authentication_error) {
  if (authentication_error.has_value()) {
    LOG(ERROR) << "An error happened during the attempt to validate "
                  "the password: "
               << authentication_error.value().get_cryptohome_error();
    user_context_ = std::move(user_context);
    UpdateState(
        LocalAuthenticationRequestViewState::kError, default_title_,
        l10n_util::GetStringUTF16(IDS_ASH_LOGIN_ERROR_AUTHENTICATING_PWD));
    ClearInput();
    NotifyAccessibilityEvent(ax::mojom::Event::kAlert,
                             true /*send_native_event*/);
    SetInputEnabled(true);
  } else {
    LocalAuthenticationRequestWidget::Get()->Close(true /* success */,
                                                   std::move(user_context));
  }
}

void LocalAuthenticationRequestView::OnDescriptionLabelTextChanged() {
  UpdateAccessibleName();
}

void LocalAuthenticationRequestView::UpdateAccessibleName() {
  if (description_label_->GetText().empty()) {
    GetViewAccessibility().SetName(
        std::string(), ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
  } else {
    GetViewAccessibility().SetName(description_label_->GetText());
  }
}

void LocalAuthenticationRequestView::OnInputTextChanged(bool is_empty) {}

}  // namespace ash
