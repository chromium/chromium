// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_feature_tour.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/bubble/bubble_utils.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/pill_button.h"
#include "ash/style/typography.h"
#include "base/functional/callback.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace {

constexpr int kDialogBorderRadius = 20;
// The insets from the border to the contents inside.
constexpr auto kContentsInsets = gfx::Insets::TLBR(32, 32, 28, 32);
constexpr auto kIllustrationSize = gfx::Size(512, 236);
// Margin between the illustration and the heading text.
constexpr int kHeadingTextTopMargin = 32;
// Margin between the heading text and the body text.
constexpr int kBodyTextTopMargin = 16;
// Margin between the body text and the buttons.
constexpr int kButtonRowTopMargin = 32;
// Margin between the two buttons.
constexpr int kBetweenButtonMargin = 8;

class FeatureTourBubbleView : public views::WidgetDelegate,
                              public views::FlexLayoutView {
  METADATA_HEADER(FeatureTourBubbleView, views::FlexLayoutView)

 public:
  FeatureTourBubbleView(base::RepeatingClosure completion_callback) {
    // TODO: b/343599950 - Replace placeholder strings.
    views::Builder<views::FlexLayoutView>(this)
        .SetOrientation(views::LayoutOrientation::kVertical)
        .SetInteriorMargin(kContentsInsets)
        .SetBackground(views::CreateThemedRoundedRectBackground(
            cros_tokens::kCrosSysDialogContainer, kDialogBorderRadius))
        .AddChildren(
            views::Builder<views::ImageView>().SetImageSize(kIllustrationSize),
            views::Builder<views::Label>(
                bubble_utils::CreateLabel(TypographyToken::kCrosDisplay7,
                                          u"Placeholder",
                                          cros_tokens::kCrosSysOnSurface))
                .SetMultiLine(true)
                .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                .SetProperty(views::kMarginsKey,
                             gfx::Insets::TLBR(kHeadingTextTopMargin, 0, 0, 0)),
            views::Builder<views::Label>(
                bubble_utils::CreateLabel(
                    TypographyToken::kCrosBody1, u"Placeholder",
                    cros_tokens::kCrosSysOnSurfaceVariant))
                .SetMultiLine(true)
                .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                .SetProperty(views::kMarginsKey,
                             gfx::Insets::TLBR(kBodyTextTopMargin, 0, 0, 0)),
            views::Builder<views::FlexLayoutView>()
                .SetProperty(views::kMarginsKey,
                             gfx::Insets::TLBR(kButtonRowTopMargin, 0, 0, 0))
                .SetOrientation(views::LayoutOrientation::kHorizontal)
                .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
                .AddChildren(
                    views::Builder<PillButton>(std::make_unique<PillButton>(
                        PillButton::PressedCallback(),
                        l10n_util::GetStringUTF16(
                            IDS_PICKER_FEATURE_TOUR_LEARN_MORE_BUTTON_LABEL),
                        PillButton::Type::kSecondaryWithoutIcon)),
                    views::Builder<PillButton>(
                        std::make_unique<PillButton>(
                            // base::Unretained is safe here since the Widget
                            // owns the View.
                            base::BindRepeating(
                                &FeatureTourBubbleView::CloseWidget,
                                base::Unretained(this))
                                .Then(std::move(completion_callback)),
                            l10n_util::GetStringUTF16(
                                IDS_PICKER_FEATURE_TOUR_GOT_IT_BUTTON_LABEL),
                            PillButton::Type::kPrimaryWithoutIcon))
                        .CopyAddressTo(&complete_button_)
                        .SetProperty(
                            views::kMarginsKey,
                            gfx::Insets::TLBR(0, kBetweenButtonMargin, 0, 0))))
        .BuildChildren();
  }

  FeatureTourBubbleView(const FeatureTourBubbleView&) = delete;
  FeatureTourBubbleView& operator=(const FeatureTourBubbleView&) = delete;
  ~FeatureTourBubbleView() override = default;

  views::Button* complete_button() { return complete_button_; }

  View* GetContentsView() override { return this; }

  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override {
    auto frame =
        std::make_unique<views::BubbleFrameView>(gfx::Insets(), gfx::Insets());
    auto border = std::make_unique<views::BubbleBorder>(
        views::BubbleBorder::NONE, views::BubbleBorder::DIALOG_SHADOW);
    border->SetCornerRadius(kDialogBorderRadius);
    frame->SetBubbleBorder(std::move(border));
    return frame;
  }

 private:
  void CloseWidget() {
    if (views::Widget* widget = views::View::GetWidget(); widget != nullptr) {
      widget->CloseWithReason(
          views::Widget::ClosedReason::kAcceptButtonClicked);
    }
  }

  raw_ptr<views::Button> complete_button_ = nullptr;
};

BEGIN_METADATA(FeatureTourBubbleView)
END_METADATA

BEGIN_VIEW_BUILDER(/*no export*/, FeatureTourBubbleView, views::FlexLayoutView)
END_VIEW_BUILDER

}  // namespace
}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::FeatureTourBubbleView)

namespace ash {
namespace {

std::unique_ptr<views::Widget> CreateWidget(
    base::RepeatingClosure completion_callback) {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.delegate = new FeatureTourBubbleView(std::move(completion_callback));
  params.name = "PickerFeatureTourWidget";

  auto widget = std::make_unique<views::Widget>(std::move(params));
  widget->GetLayer()->SetFillsBoundsOpaquely(false);
  return widget;
}

}  // namespace

PickerFeatureTour::PickerFeatureTour() = default;

PickerFeatureTour::~PickerFeatureTour() {
  if (widget_) {
    widget_->CloseNow();
  }
}

void PickerFeatureTour::Show(base::RepeatingClosure completion_callback) {
  widget_ = CreateWidget(std::move(completion_callback));
  widget_->Show();
}

views::Button* PickerFeatureTour::complete_button_for_testing() {
  if (!widget_) {
    return nullptr;
  }

  auto* bubble_view =
      static_cast<FeatureTourBubbleView*>(widget_->GetContentsView());
  return bubble_view ? bubble_view->complete_button() : nullptr;
}

views::Widget* PickerFeatureTour::widget_for_testing() {
  return widget_.get();
}

}  // namespace ash
