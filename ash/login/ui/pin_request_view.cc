// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/pin_request_view.h"

#include "ash/login/ui/arrow_button_view.h"
#include "ash/login/ui/login_pin_view.h"
#include "ash/login/ui/pin_request_widget.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/vector_icons.h"

namespace ash {

namespace {

constexpr int kPinRequestViewWidthDp = 340;
constexpr int kPinKeyboardHeightDp = 224;
constexpr int kPinRequestViewRoundedCornerRadiusDp = 8;
constexpr int kPinRequestViewVerticalInsetDp = 8;
// Inset for all elements except the back button.
constexpr int kPinRequestViewMainHorizontalInsetDp = 36;
// Minimum inset (= back button inset).
constexpr int kPinRequestViewHorizontalInsetDp = 8;

constexpr int kCrossSizeDp = 20;
constexpr int kBackButtonSizeDp = 36;
constexpr int kLockIconSizeDp = 24;
constexpr int kBackButtonLockIconVerticalOverlapDp = 8;
constexpr int kHeaderHeightDp =
    kBackButtonSizeDp + kLockIconSizeDp - kBackButtonLockIconVerticalOverlapDp;

constexpr int kIconToTitleDistanceDp = 24;
constexpr int kTitleToDescriptionDistanceDp = 8;
constexpr int kDescriptionToAccessCodeDistanceDp = 32;
constexpr int kAccessCodeToPinKeyboardDistanceDp = 16;
constexpr int kPinKeyboardToFooterDistanceDp = 16;
constexpr int kSubmitButtonBottomMarginDp = 28;

constexpr int kTitleFontSizeDeltaDp = 4;
constexpr int kTitleLineWidthDp = 268;
constexpr int kTitleLineHeightDp = 24;
constexpr int kTitleMaxLines = 4;
constexpr int kDescriptionFontSizeDeltaDp = 0;
constexpr int kDescriptionLineWidthDp = 268;
constexpr int kDescriptionTextLineHeightDp = 18;
constexpr int kDescriptionMaxLines = 4;

constexpr int kArrowButtonSizeDp = 48;

constexpr int kPinRequestViewMinimumHeightDp =
    kPinRequestViewMainHorizontalInsetDp + kLockIconSizeDp +
    kIconToTitleDistanceDp + kTitleToDescriptionDistanceDp +
    kDescriptionToAccessCodeDistanceDp +
    AccessCodeInput::kAccessCodeInputFieldHeightDp +
    kAccessCodeToPinKeyboardDistanceDp + kPinKeyboardToFooterDistanceDp +
    kArrowButtonSizeDp + kPinRequestViewMainHorizontalInsetDp;  // = 266

bool IsTabletMode() {
  return Shell::Get()->tablet_mode_controller()->InTabletMode();
}

}  // namespace

PinRequest::PinRequest() = default;
PinRequest::PinRequest(PinRequest&&) = default;
PinRequest& PinRequest::operator=(PinRequest&&) = default;
PinRequest::~PinRequest() = default;

// Label button that displays focus ring.
class PinRequestView::FocusableLabelButton : public views::LabelButton {
 public:
  FocusableLabelButton(PressedCallback callback, const std::u16string& text)
      : views::LabelButton(std::move(callback), text) {
    SetInstallFocusRingOnFocus(true);
    views::FocusRing::Get(this)->SetColorId(ui::kColorAshFocusRing);
    SetFocusBehavior(FocusBehavior::ALWAYS);
  }

  FocusableLabelButton(const FocusableLabelButton&) = delete;
  FocusableLabelButton& operator=(const FocusableLabelButton&) = delete;
  ~FocusableLabelButton() override = default;
};

PinRequestView::TestApi::TestApi(PinRequestView* view) : view_(view) {
  DCHECK(view_);
}

PinRequestView::TestApi::~TestApi() = default;

LoginButton* PinRequestView::TestApi::back_button() {
  return view_->back_button_;
}

views::Label* PinRequestView::TestApi::title_label() {
  return view_->title_label_;
}

views::Label* PinRequestView::TestApi::description_label() {
  return view_->description_label_;
}

views::View* PinRequestView::TestApi::access_code_view() {
  return view_->access_code_view_;
}

views::LabelButton* PinRequestView::TestApi::help_button() {
  return view_->help_button_;
}

ArrowButtonView* PinRequestView::TestApi::submit_button() {
  return view_->submit_button_;
}

LoginPinView* PinRequestView::TestApi::pin_keyboard_view() {
  return view_->pin_keyboard_view_;
}

views::Textfield* PinRequestView::TestApi::GetInputTextField(int index) {
  return FixedLengthCodeInput::TestApi(
             static_cast<FixedLengthCodeInput*>(view_->access_code_view_))
      .GetInputTextField(index);
}

PinRequestViewState PinRequestView::TestApi::state() const {
  return view_->state_;
}

// static
SkColor PinRequestView::GetChildUserDialogColor(bool using_blur) {
  return AshColorProvider::Get()->GetBaseLayerColor(
      using_blur ? AshColorProvider::BaseLayerType::kTransparent80
                 : AshColorProvider::BaseLayerType::kOpaque);
}

// TODO(crbug.com/1061008): Make dialog look good on small screens with high
// zoom factor.
PinRequestView::PinRequestView(PinRequest request, Delegate* delegate)
    : delegate_(delegate),
      on_pin_request_done_(std::move(request.on_pin_request_done)),
      pin_keyboard_always_enabled_(request.pin_keyboard_always_enabled),
      default_title_(request.title),
      default_description_(request.description),
      default_accessible_title_(request.accessible_title.empty()
                                    ? request.title
                                    : request.accessible_title) {
  // MODAL_TYPE_SYSTEM is used to get a semi-transparent background behind the
  // pin request view, when it is used directly on a widget. The overlay
  // consumes all the inputs from the user, so that they can only interact with
  // the pin request view while it is visible.
  SetModalType(ui::MODAL_TYPE_SYSTEM);

  // Main view contains all other views aligned vertically and centered.
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(kPinRequestViewVerticalInsetDp,
                      kPinRequestViewHorizontalInsetDp),
      0);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  SetLayoutManager(std::move(layout));

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(kPinRequestViewRoundedCornerRadiusDp));
  layer()->SetBackgroundBlur(ShelfConfig::Get()->shelf_blur_radius());

  const int child_view_width =
      kPinRequestViewWidthDp - 2 * kPinRequestViewMainHorizontalInsetDp;

  // Header view which contains the back button that is aligned top right and
  // the lock icon which is in the bottom center.
  auto header_layout = std::make_unique<views::FillLayout>();
  auto* header = new NonAccessibleView();
  header->SetLayoutManager(std::move(header_layout));
  AddChildView(header);
  auto* header_spacer = new NonAccessibleView();
  header_spacer->SetPreferredSize(gfx::Size(0, kHeaderHeightDp));
  header->AddChildView(header_spacer);

  // Main view icon.
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

  views::ImageView* icon = new views::ImageView();
  icon->SetPreferredSize(gfx::Size(kLockIconSizeDp, kLockIconSizeDp));
  icon->SetImage(gfx::CreateVectorIcon(
      kPinRequestLockIcon,
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary)));
  icon_view->AddChildView(icon);

  // Back button. Note that it should be the last view added to |header| in
  // order to be clickable.
  auto* back_button_view = new NonAccessibleView();
  back_button_view->SetPreferredSize(
      gfx::Size(child_view_width + 2 * (kPinRequestViewMainHorizontalInsetDp -
                                        kPinRequestViewHorizontalInsetDp),
                kHeaderHeightDp));
  auto back_button_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 0);
  back_button_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);
  back_button_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  back_button_view->SetLayoutManager(std::move(back_button_layout));
  header->AddChildView(back_button_view);

  back_button_ = new LoginButton(
      base::BindRepeating(&PinRequestView::OnBack, base::Unretained(this)));
  back_button_->SetPreferredSize(
      gfx::Size(kBackButtonSizeDp, kBackButtonSizeDp));
  back_button_->SetBackground(
      views::CreateSolidBackground(SK_ColorTRANSPARENT));
  back_button_->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(
          views::kIcCloseIcon, kCrossSizeDp,
          AshColorProvider::Get()->GetContentLayerColor(
              AshColorProvider::ContentLayerType::kIconColorPrimary)));
  back_button_->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  back_button_->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  back_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_BACK_BUTTON_ACCESSIBLE_NAME));
  back_button_->SetFocusBehavior(FocusBehavior::ALWAYS);
  back_button_view->AddChildView(back_button_);

  auto add_spacer = [&](int height) {
    auto* spacer = new NonAccessibleView();
    spacer->SetPreferredSize(gfx::Size(0, height));
    AddChildView(spacer);
  };

  add_spacer(kIconToTitleDistanceDp);

  auto decorate_label = [](views::Label* label) {
    label->SetSubpixelRenderingEnabled(false);
    label->SetAutoColorReadabilityEnabled(false);
    label->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary));
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
  AddChildView(title_label_);

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
  AddChildView(description_label_);

  add_spacer(kDescriptionToAccessCodeDistanceDp);

  // Access code input view.
  if (request.pin_length.has_value()) {
    CHECK_GT(request.pin_length.value(), 0);
    access_code_view_ = AddChildView(std::make_unique<FixedLengthCodeInput>(
        request.pin_length.value(),
        base::BindRepeating(&PinRequestView::OnInputChange,
                            base::Unretained(this)),
        base::BindRepeating(&PinRequestView::SubmitCode,
                            base::Unretained(this)),
        base::BindRepeating(&PinRequestView::OnBack, base::Unretained(this)),
        request.obscure_pin));
    access_code_view_->SetFocusBehavior(FocusBehavior::ALWAYS);
  } else {
    auto flex_code_input = std::make_unique<FlexCodeInput>(
        base::BindRepeating(&PinRequestView::OnInputChange,
                            base::Unretained(this), false),
        base::BindRepeating(&PinRequestView::SubmitCode,
                            base::Unretained(this)),
        base::BindRepeating(&PinRequestView::OnBack, base::Unretained(this)),
        request.obscure_pin);
    flex_code_input->SetAccessibleName(default_accessible_title_);
    access_code_view_ = AddChildView(std::move(flex_code_input));
  }

  add_spacer(kAccessCodeToPinKeyboardDistanceDp);

  // Pin keyboard. Note that the keyboard's own submit button is disabled via
  // passing a null |on_submit| callback.
  pin_keyboard_view_ =
      new LoginPinView(LoginPinView::Style::kAlphanumeric,
                       base::BindRepeating(&AccessCodeInput::InsertDigit,
                                           base::Unretained(access_code_view_)),
                       base::BindRepeating(&AccessCodeInput::Backspace,
                                           base::Unretained(access_code_view_)),
                       /*on_submit=*/LoginPinView::OnPinSubmit());
  // Backspace key is always enabled and |access_code_| field handles it.
  pin_keyboard_view_->OnPasswordTextChanged(false);
  AddChildView(pin_keyboard_view_);

  add_spacer(kPinKeyboardToFooterDistanceDp);

  // Footer view contains help text button aligned to its start, submit
  // button aligned to its end and spacer view in between.
  auto* footer = new NonAccessibleView();
  footer->SetPreferredSize(gfx::Size(child_view_width, kArrowButtonSizeDp));
  auto* bottom_layout =
      footer->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 0));
  AddChildView(footer);

  help_button_ = new FocusableLabelButton(
      base::BindRepeating(
          [](PinRequestView* view) { view->delegate_->OnHelp(); }, this),
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_PIN_REQUEST_HELP));
  help_button_->SetPaintToLayer();
  help_button_->layer()->SetFillsBoundsOpaquely(false);
  help_button_->SetTextSubpixelRenderingEnabled(false);
  help_button_->SetEnabledTextColors(
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kTextColorPrimary));
  help_button_->SetVisible(request.help_button_enabled);
  footer->AddChildView(help_button_);

  auto* horizontal_spacer = new NonAccessibleView();
  footer->AddChildView(horizontal_spacer);
  bottom_layout->SetFlexForView(horizontal_spacer, 1);

  submit_button_ = new ArrowButtonView(
      base::BindRepeating(&PinRequestView::SubmitCode, base::Unretained(this)),
      kArrowButtonSizeDp);
  submit_button_->SetPreferredSize(
      gfx::Size(kArrowButtonSizeDp, kArrowButtonSizeDp));
  submit_button_->SetEnabled(false);
  submit_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_SUBMIT_BUTTON_ACCESSIBLE_NAME));
  submit_button_->SetFocusBehavior(FocusBehavior::ALWAYS);
  footer->AddChildView(submit_button_);
  add_spacer(kSubmitButtonBottomMarginDp);

  pin_keyboard_view_->SetVisible(PinKeyboardVisible());

  tablet_mode_observation_.Observe(Shell::Get()->tablet_mode_controller());

  SetPreferredSize(GetPinRequestViewSize());
}

PinRequestView::~PinRequestView() = default;

void PinRequestView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(GetChildUserDialogColor(true));
  canvas->DrawRoundRect(GetContentsBounds(),
                        kPinRequestViewRoundedCornerRadiusDp, flags);
}

void PinRequestView::RequestFocus() {
  access_code_view_->RequestFocus();
}

gfx::Size PinRequestView::CalculatePreferredSize() const {
  return GetPinRequestViewSize();
}

views::View* PinRequestView::GetInitiallyFocusedView() {
  return access_code_view_;
}

std::u16string PinRequestView::GetAccessibleWindowTitle() const {
  return default_accessible_title_;
}

void PinRequestView::OnTabletModeStarted() {
  if (!pin_keyboard_always_enabled_) {
    VLOG(1) << "Showing PIN keyboard in PinRequestView";
    pin_keyboard_view_->SetVisible(true);
    // This will trigger ChildPreferredSizeChanged in parent view and Layout()
    // in view. As the result whole hierarchy will go through re-layout.
    UpdatePreferredSize();
  }
}

void PinRequestView::OnTabletModeEnded() {
  if (!pin_keyboard_always_enabled_) {
    VLOG(1) << "Hiding PIN keyboard in PinRequestView";
    DCHECK(pin_keyboard_view_);
    pin_keyboard_view_->SetVisible(false);
    // This will trigger ChildPreferredSizeChanged in parent view and Layout()
    // in view. As the result whole hierarchy will go through re-layout.
    UpdatePreferredSize();
  }
}

void PinRequestView::OnTabletControllerDestroyed() {
  tablet_mode_observation_.Reset();
}

void PinRequestView::SubmitCode() {
  absl::optional<std::string> code = access_code_view_->GetCode();
  DCHECK(code.has_value());

  SubmissionResult result = delegate_->OnPinSubmitted(*code);
  switch (result) {
    case SubmissionResult::kPinAccepted: {
      std::move(on_pin_request_done_).Run(true /* success */);
      return;
    }
    case SubmissionResult::kPinError: {
      // Caller is expected to call UpdateState() to allow for customization of
      // error messages.
      return;
    }
    case SubmissionResult::kSubmitPending: {
      // Waiting on validation result - do nothing for now.
      return;
    }
  }
}

void PinRequestView::OnBack() {
  delegate_->OnBack();
  if (PinRequestWidget::Get()) {
    PinRequestWidget::Get()->Close(false /* success */);
  }
}

void PinRequestView::UpdateState(PinRequestViewState state,
                                 const std::u16string& title,
                                 const std::u16string& description) {
  state_ = state;
  title_label_->SetText(title);
  description_label_->SetText(description);
  UpdatePreferredSize();
  switch (state_) {
    case PinRequestViewState::kNormal: {
      const SkColor kTextColor = AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kTextColorPrimary);
      access_code_view_->SetInputColor(kTextColor);
      title_label_->SetEnabledColor(kTextColor);
      return;
    }
    case PinRequestViewState::kError: {
      const SkColor kErrorColor = AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kTextColorAlert);
      access_code_view_->SetInputColor(kErrorColor);
      title_label_->SetEnabledColor(kErrorColor);
      // Read out the error.
      title_label_->NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
      return;
    }
  }
}

void PinRequestView::ClearInput() {
  access_code_view_->ClearInput();
}

void PinRequestView::SetInputEnabled(bool input_enabled) {
  access_code_view_->SetInputEnabled(input_enabled);
}

void PinRequestView::UpdatePreferredSize() {
  SetPreferredSize(CalculatePreferredSize());
  if (GetWidget()) {
    GetWidget()->CenterWindow(GetPreferredSize());
  }
}

void PinRequestView::FocusSubmitButton() {
  submit_button_->RequestFocus();
}

void PinRequestView::OnInputChange(bool last_field_active, bool complete) {
  if (state_ == PinRequestViewState::kError) {
    UpdateState(PinRequestViewState::kNormal, default_title_,
                default_description_);
  }

  submit_button_->SetEnabled(complete);

  if (complete && last_field_active) {
    if (auto_submit_enabled_) {
      auto_submit_enabled_ = false;
      SubmitCode();
      return;
    }

    // Moving focus is delayed by using PostTask to allow for proper
    // a11y announcements.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&PinRequestView::FocusSubmitButton,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

void PinRequestView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  views::View::GetAccessibleNodeData(node_data);
  node_data->role = ax::mojom::Role::kDialog;
  node_data->SetName(default_accessible_title_);
}

// If |pin_keyboard_always_enabled_| is not set, pin keyboard is only shown in
// tablet mode.
bool PinRequestView::PinKeyboardVisible() const {
  return pin_keyboard_always_enabled_ || IsTabletMode();
}

gfx::Size PinRequestView::GetPinRequestViewSize() const {
  int height =
      kPinRequestViewMinimumHeightDp +
      std::min(static_cast<int>(title_label_->GetRequiredLines()),
               kTitleMaxLines) *
          kTitleLineHeightDp +
      std::min(static_cast<int>(description_label_->GetRequiredLines()),
               kDescriptionMaxLines) *
          kDescriptionTextLineHeightDp;
  if (PinKeyboardVisible()) {
    height += kPinKeyboardHeightDp;
  }
  return gfx::Size(kPinRequestViewWidthDp, height);
}

}  // namespace ash
