// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/system_dialog_delegate_view.h"

#include <memory>

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/pill_button.h"
#include "ash/style/typography.h"
#include "base/functional/bind.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view_class_properties.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// The default color IDs of the dialog.
constexpr ui::ColorId kBackgroundColorId = cros_tokens::kCrosSysBaseElevated;
constexpr ui::ColorId kBodyColorId = cros_tokens::kCrosSysOnSurfaceVariant;
constexpr ui::ColorId kIconColorId = cros_tokens::kCrosSysPrimary;
constexpr ui::ColorId kTitleColorId = cros_tokens::kCrosSysOnSurface;

// The default layout parameters of the dialog.
constexpr gfx::Size kMinimumDialogSize = gfx::Size(296, 144);
constexpr gfx::Size kMaximumDialogSize = gfx::Size(512, 600);
constexpr int kRoundedCornerRadius = 20;
constexpr gfx::Insets kBorderInsets = gfx::Insets::TLBR(32, 32, 28, 32);
constexpr int kIconSize = 32;
constexpr int kIconBottomPadding = 20;
constexpr int kTitleBottomPadding = 16;
constexpr int kDefaultAdditionalContentTopPadding = 32;
constexpr int kButtonContainerTopPadding = 32;
constexpr int kButtonSpacing = 8;
constexpr int kMinimumAdditionalButtonPadding = 80;

// Typical sizes of a dialog.
constexpr int kDialogWidthLarge = 512;
constexpr int kDialogWidthMedium = 359;
constexpr int kDialogWidthSmall = 296;

// The host window sizes that will change the resizing rule of the dialog.
constexpr int kHostWidthLarge = 672;
constexpr int kHostWidthMedium = 520;
constexpr int kHostWidthSmall = 424;
constexpr int kHostWidthXSmall = 400;

// Padding between the dialog and the host window.
constexpr int kDialogHostPaddingLarge = 80;
constexpr int kDialogHostPaddingSmall = 32;

// The position of the additional content in the dialog child views.
constexpr int kAdditionalContentID = 3;

// The default fonts of the title and description.
constexpr TypographyToken kTitleFont = TypographyToken::kCrosDisplay7;
constexpr TypographyToken kBodyFont = TypographyToken::kCrosBody1;

// Sets margins and flex layout specs to the view.
void SetViewLayoutSpecs(
    views::View* view,
    const gfx::Insets& margins = gfx::Insets(),
    const views::FlexSpecification flex_spec = views::FlexSpecification()) {
  view->SetProperty(views::kMarginsKey, margins);
  view->SetProperty(views::kFlexBehaviorKey, flex_spec);
}

// Gets the host window of the dialog.
aura::Window* GetDialogHostWindow(const views::Widget* dialog_widget) {
  if (!dialog_widget) {
    return nullptr;
  }

  // Return transient parent as the host window if exists. Otherwise, return the
  // default parent.
  auto* dialog_window = dialog_widget->GetNativeWindow();
  auto* transient_parent = wm::GetTransientParent(dialog_window);
  return transient_parent ? transient_parent : dialog_window->parent();
}

}  // namespace

//------------------------------------------------------------------------------
// SystemDialogDelegateView::ButtonContainer:
// The container includes an accept button and a cancel button. The buttons are
// awlays at the right bottom corner of the dialog. The container also allows an
// additional view to be added at the left side. Please refer to the example in
// the header file for the container layout.
class SystemDialogDelegateView::ButtonContainer : public views::FlexLayoutView {
 public:
  METADATA_HEADER(ButtonContainer);

  explicit ButtonContainer(SystemDialogDelegateView* dialog_view)
      : cancel_button_(AddChildView(std::make_unique<PillButton>(
            base::BindRepeating(&SystemDialogDelegateView::Cancel,
                                base::Unretained(dialog_view)),
            l10n_util::GetStringUTF16(IDS_APP_CANCEL),
            PillButton::Type::kSecondaryWithoutIcon))),
        accept_button_(AddChildView(std::make_unique<PillButton>(
            base::BindRepeating(&SystemDialogDelegateView::Accept,
                                base::Unretained(dialog_view)),
            l10n_util::GetStringUTF16(IDS_APP_OK),
            PillButton::Type::kPrimaryWithoutIcon))) {
    SetOrientation(views::LayoutOrientation::kHorizontal);
    SetMainAxisAlignment(views::LayoutAlignment::kEnd);
    SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

    SetViewLayoutSpecs(cancel_button_,
                       gfx::Insets::TLBR(0, 0, 0, kButtonSpacing));
  }

  ButtonContainer(const ButtonContainer&) = delete;
  ButtonContainer& operator=(const ButtonContainer&) = delete;
  ~ButtonContainer() override = default;

  void SetAcceptText(const std::u16string& accept_text) {
    accept_button_->SetText(accept_text);
  }

  void SetCancelText(const std::u16string& cancel_text) {
    cancel_button_->SetText(cancel_text);
  }

  void SetAdditionalView(std::unique_ptr<views::View> additional_view) {
    if (additional_view_) {
      RemoveChildViewT(additional_view_);
    }

    // Create a place holder view to fill the space between the cancel button
    // and the additional view.
    if (!place_holder_view_) {
      place_holder_view_ = AddChildViewAt(std::make_unique<views::View>(), 0);
      SetViewLayoutSpecs(
          place_holder_view_,
          gfx::Insets::TLBR(0, 0, 0, kMinimumAdditionalButtonPadding),
          views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kUnbounded));
    }
    additional_view_ = AddChildViewAt(std::move(additional_view), 0);
  }

 private:
  // Owned by the container.
  base::raw_ptr<PillButton> cancel_button_ = nullptr;
  base::raw_ptr<PillButton> accept_button_ = nullptr;
  base::raw_ptr<views::View> additional_view_ = nullptr;
  // The view used to fill the free spaces between the additional view and
  // cancel button.
  base::raw_ptr<views::View> place_holder_view_ = nullptr;
};

BEGIN_METADATA(SystemDialogDelegateView, ButtonContainer, views::FlexLayoutView)
END_METADATA

//------------------------------------------------------------------------------
// SystemDialogDelegateView:
SystemDialogDelegateView::SystemDialogDelegateView() {
  // Set border and background.
  SetBorder(views::CreatePaddedBorder(
      std::make_unique<views::HighlightBorder>(
          kRoundedCornerRadius,
          views::HighlightBorder::Type::kHighlightBorderOnShadow),
      kBorderInsets));
  SetBackground(views::CreateThemedRoundedRectBackground(kBackgroundColorId,
                                                         kRoundedCornerRadius));

  // Set shadow.
  shadow_ = SystemShadow::CreateShadowOnNinePatchLayerForView(
      this, SystemShadow::Type::kElevation12);
  shadow_->SetRoundedCornerRadius(kRoundedCornerRadius);

  // Use flex layout.
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetCollapseMargins(true);

  icon_ = AddChildView(std::make_unique<views::ImageView>());
  SetViewLayoutSpecs(icon_, gfx::Insets::TLBR(0, 0, kIconBottomPadding, 0));
  icon_->SetProperty(views::kCrossAxisAlignmentKey,
                     views::LayoutAlignment::kStart);
  icon_->SetVisible(false);

  // Configure icon, title, description, and button container with pre-defined
  // layout.
  auto* typography_provider = TypographyProvider::Get();
  title_ = AddChildView(std::make_unique<views::Label>());
  SetViewLayoutSpecs(title_, gfx::Insets::TLBR(0, 0, kTitleBottomPadding, 0));
  typography_provider->StyleLabel(kTitleFont, *title_);
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_->SetAutoColorReadabilityEnabled(false);
  title_->SetEnabledColorId(kTitleColorId);
  title_->SetVisible(false);

  description_ = AddChildView(std::make_unique<views::Label>());
  SetViewLayoutSpecs(
      description_, gfx::Insets(),
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kUnbounded, true,
                               views::MinimumFlexSizeRule::kPreferred));
  typography_provider->StyleLabel(kBodyFont, *description_);
  description_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  description_->SetMultiLine(true);
  description_->SetAllowCharacterBreak(true);
  description_->SetAutoColorReadabilityEnabled(false);
  description_->SetEnabledColorId(kBodyColorId);
  description_->SetVisible(false);

  button_container_ = AddChildView(std::make_unique<ButtonContainer>(this));
  SetViewLayoutSpecs(
      button_container_, gfx::Insets::TLBR(kButtonContainerTopPadding, 0, 0, 0),
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kUnbounded));

  // Register the close callback.
  RegisterWindowWillCloseCallback(
      base::BindOnce(&SystemDialogDelegateView::Close, base::Unretained(this)));
}

SystemDialogDelegateView::~SystemDialogDelegateView() = default;

void SystemDialogDelegateView::SetIcon(const gfx::VectorIcon& icon) {
  icon_->SetImage(
      ui::ImageModel::FromVectorIcon(icon, kIconColorId, kIconSize));
  icon_->SetVisible(true);
}

void SystemDialogDelegateView::SetTitleText(const std::u16string& title) {
  title_->SetText(title);
  title_->SetVisible(!title.empty());
}

void SystemDialogDelegateView::SetDescription(
    const std::u16string& description) {
  description_->SetText(description);
  description_->SetVisible(!description.empty());
}

void SystemDialogDelegateView::SetDescriptionAccessibleName(
    const std::u16string& accessible_name) {
  description_->SetAccessibleName(accessible_name);
}

void SystemDialogDelegateView::SetAcceptButtonText(
    const std::u16string& accept_text) {
  button_container_->SetAcceptText(accept_text);
}

void SystemDialogDelegateView::SetCancelButtonText(
    const std::u16string& cancel_text) {
  button_container_->SetCancelText(cancel_text);
}

void SystemDialogDelegateView::SetAdditionalContentCrossAxisAlignment(
    views::LayoutAlignment alignment) {
  DCHECK(additional_content_);
  auto* cross_aligment =
      additional_content_->GetProperty(views::kCrossAxisAlignmentKey);
  if (!cross_aligment || *cross_aligment != alignment) {
    additional_content_->SetProperty(views::kCrossAxisAlignmentKey, alignment);
  }
}

gfx::Size SystemDialogDelegateView::CalculatePreferredSize() const {
  auto* host_window = GetDialogHostWindow(GetWidget());
  // If the delegate view is not added to a widget or parented to a host window,
  // return the default preferred size.
  if (!host_window) {
    return views::WidgetDelegateView::CalculatePreferredSize();
  }

  // Otherwise, calculate the preferred size according to its host window size.
  const int host_width = host_window->GetBoundsInScreen().width();
  // The resizing rules of the dialog are as follows:
  // - When the host window width is larger than `kHostWidthLarge`, the dialog
  // width would remain at `kDialogWidthLarge`.
  // - When the host window width is between `kHostWidthMedium` and
  // `kHostWidthLarge`, the dialog width will decrease but maintain a padding
  // of `kDialogHostPaddingLarge` on both sides.
  // - When the host window width is between `kHostWidthSmall` and
  // `kHostWidthMedium`, the dialog width would remain at `kDialogWidthMedium`.
  // - When the host window width is less than `kHostWidthXSmall`, the dialog
  // width will decrease but maintain a padding of `kDialogHostPaddingSmall` on
  // both sides.
  // - The dialog minimum width is `kDialogWidthSmall`.
  int dialog_width = kDialogWidthSmall;
  if (host_width >= kHostWidthLarge) {
    dialog_width = kDialogWidthLarge;
  } else if (host_width >= kHostWidthMedium) {
    dialog_width = host_width - kDialogHostPaddingLarge * 2;
  } else if (host_width >= kHostWidthSmall) {
    dialog_width = kDialogWidthMedium;
  } else if (host_width >= kHostWidthXSmall) {
    dialog_width = host_width - kDialogHostPaddingSmall * 2;
  }

  return gfx::Size(dialog_width, GetHeightForWidth(dialog_width));
}

gfx::Size SystemDialogDelegateView::GetMinimumSize() const {
  return kMinimumDialogSize;
}

gfx::Size SystemDialogDelegateView::GetMaximumSize() const {
  return kMaximumDialogSize;
}

void SystemDialogDelegateView::OnWidgetInitialized() {
  UpdateDialogSize();
}

void SystemDialogDelegateView::OnWorkAreaChanged() {
  UpdateDialogSize();
}

void SystemDialogDelegateView::UpdateDialogSize() {
  if (auto* widget = GetWidget()) {
    widget->CenterWindow(GetPreferredSize());
  }
}

void SystemDialogDelegateView::SetAdditionalContentInternal(
    std::unique_ptr<views::View> view) {
  // If there is an additional content, remove it.
  if (additional_content_) {
    RemoveChildViewT(additional_content_);
  }

  // Add additional content and move it to the specific position.
  additional_content_ = AddChildView(std::move(view));
  ReorderChildView(additional_content_, kAdditionalContentID);

  // If there is no preset margins or the top margin is 0, set the top margin
  // with the default padding.
  auto* margins = additional_content_->GetProperty(views::kMarginsKey);
  if (!margins) {
    additional_content_->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(kDefaultAdditionalContentTopPadding, 0, 0, 0));
  } else if (!margins->top()) {
    margins->set_top(kDefaultAdditionalContentTopPadding);
  }

  additional_content_->SetProperty(views::kCrossAxisAlignmentKey,
                                   views::LayoutAlignment::kCenter);
}

void SystemDialogDelegateView::SetAdditionalViewInButtonRowInternal(
    std::unique_ptr<views::View> view) {
  button_container_->SetAdditionalView(std::move(view));
}

void SystemDialogDelegateView::Accept() {
  if (!closing_dialog_) {
    RunCallbackAndCloseDialog(
        std::move(accept_callback_),
        views::Widget::ClosedReason::kAcceptButtonClicked);
  }
}

void SystemDialogDelegateView::Cancel() {
  if (!closing_dialog_) {
    RunCallbackAndCloseDialog(
        std::move(cancel_callback_),
        views::Widget::ClosedReason::kCancelButtonClicked);
  }
}

void SystemDialogDelegateView::Close() {
  if (!closing_dialog_ && close_callback_) {
    std::move(close_callback_).Run();
  }
  closing_dialog_ = true;
}

void SystemDialogDelegateView::RunCallbackAndCloseDialog(
    base::OnceClosure callback,
    views::Widget::ClosedReason closed_reason) {
  CHECK(!closing_dialog_);

  if (callback) {
    std::move(callback).Run();
  }

  if (auto* widget = GetWidget()) {
    // Update the `closing_dialog_` before closing the widget.
    closing_dialog_ = true;
    widget->CloseWithReason(closed_reason);
  }
}

BEGIN_METADATA(SystemDialogDelegateView, views::WidgetDelegateView)
END_METADATA

}  // namespace ash
