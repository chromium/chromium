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
#include "build/branding_buildflags.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
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
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chromeos/ash/resources/internal/strings/grit/ash_internal_strings.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace ash {
namespace {

constexpr int kDialogBorderRadius = 20;
constexpr int kDialogWidth = 512;
constexpr auto kIllustrationSize = gfx::Size(kDialogWidth, 236);
// The insets of the main contents.
constexpr auto kMainContentInsets = gfx::Insets::TLBR(32, 32, 28, 32);
// Margin between the heading text and the body text.
constexpr int kBodyTextTopMargin = 16;
// Margin between the body text and the buttons.
constexpr int kButtonRowTopMargin = 32;
// Margin between the two buttons.
constexpr int kBetweenButtonMargin = 8;
// Pref storing whether the feature tour was completed.
constexpr char kFeatureTourCompletedPref[] =
    "ash.picker.feature_tour.completed";

bool g_feature_tour_enabled = true;

std::u16string GetHeadingText() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return l10n_util::GetStringUTF16(IDS_PICKER_FEATURE_TOUR_HEADING_TEXT);
#else
  return u"";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

std::u16string GetBodyText() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return l10n_util::GetStringUTF16(IDS_PICKER_FEATURE_TOUR_BODY_TEXT);
#else
  return u"";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

class FeatureTourBubbleView : public views::WidgetDelegate,
                              public views::BoxLayoutView {
  METADATA_HEADER(FeatureTourBubbleView, views::BoxLayoutView)

 public:
  FeatureTourBubbleView(base::RepeatingClosure completion_callback) {
    auto button_row_view =
        views::Builder<views::BoxLayoutView>()
            .SetProperty(views::kMarginsKey,
                         gfx::Insets::TLBR(kButtonRowTopMargin, 0, 0, 0))
            .SetOrientation(views::LayoutOrientation::kHorizontal)
            .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
            .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
            .SetBetweenChildSpacing(kBetweenButtonMargin)
            .AddChildren(
                views::Builder<PillButton>(std::make_unique<PillButton>(
                    PillButton::PressedCallback(),
                    l10n_util::GetStringUTF16(
                        IDS_PICKER_FEATURE_TOUR_LEARN_MORE_BUTTON_LABEL),
                    PillButton::Type::kSecondaryWithoutIcon)),
                views::Builder<PillButton>(
                    std::make_unique<PillButton>(
                        // base::Unretained is safe here since the
                        // Widget owns the View.
                        base::BindRepeating(&FeatureTourBubbleView::CloseWidget,
                                            base::Unretained(this))
                            .Then(std::move(completion_callback)),
                        l10n_util::GetStringUTF16(
                            IDS_PICKER_FEATURE_TOUR_START_BUTTON_LABEL),
                        PillButton::Type::kPrimaryWithoutIcon))
                    .CopyAddressTo(&complete_button_));

    auto main_contents_view =
        views::Builder<views::BoxLayoutView>()
            .SetOrientation(views::LayoutOrientation::kVertical)
            .SetInsideBorderInsets(kMainContentInsets)
            .AddChildren(
                views::Builder<views::Label>(
                    bubble_utils::CreateLabel(TypographyToken::kCrosDisplay7,
                                              GetHeadingText(),
                                              cros_tokens::kCrosSysOnSurface))
                    .SetMultiLine(true)
                    .SetMaximumWidth(kDialogWidth - kMainContentInsets.width())
                    .SetHorizontalAlignment(
                        gfx::HorizontalAlignment::ALIGN_LEFT),
                views::Builder<views::Label>(
                    bubble_utils::CreateLabel(
                        TypographyToken::kCrosBody1, GetBodyText(),
                        cros_tokens::kCrosSysOnSurfaceVariant))
                    .SetMultiLine(true)
                    .SetMaximumWidth(kDialogWidth - kMainContentInsets.width())
                    .SetHorizontalAlignment(
                        gfx::HorizontalAlignment::ALIGN_LEFT)
                    .SetProperty(
                        views::kMarginsKey,
                        gfx::Insets::TLBR(kBodyTextTopMargin, 0, 0, 0)),
                std::move(button_row_view));

    // TODO: b/343599950 - Add final banner image.
    views::Builder<views::BoxLayoutView>(this)
        .SetOrientation(views::LayoutOrientation::kVertical)
        .SetBackground(views::CreateThemedRoundedRectBackground(
            cros_tokens::kCrosSysDialogContainer, kDialogBorderRadius))
        .AddChildren(
            views::Builder<views::ImageView>().SetImageSize(kIllustrationSize),
            std::move(main_contents_view))
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

BEGIN_VIEW_BUILDER(/*no export*/, FeatureTourBubbleView, views::BoxLayoutView)
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

void PickerFeatureTour::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kFeatureTourCompletedPref, false);
}

void PickerFeatureTour::DisableFeatureTourForTesting() {
  g_feature_tour_enabled = false;
}

bool PickerFeatureTour::MaybeShowForFirstUse(
    PrefService* prefs,
    base::RepeatingClosure completion_callback) {
  if (!g_feature_tour_enabled) {
    return false;
  }

  auto* pref = prefs->FindPreference(kFeatureTourCompletedPref);
  // Don't show if `pref` is null (this happens in unit tests that don't call
  // `RegisterProfilePrefs`).
  if (pref == nullptr || pref->GetValue()->GetBool()) {
    return false;
  }

  widget_ = CreateWidget(std::move(completion_callback));
  widget_->Show();

  prefs->SetBoolean(kFeatureTourCompletedPref, true);
  return true;
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
