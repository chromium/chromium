// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_question_answer_view.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/icon_button.h"
#include "ash/style/system_textfield.h"
#include "ash/style/typography.h"
#include "ash/system/mahi/mahi_animation_utils.h"
#include "ash/system/mahi/mahi_constants.h"
#include "ash/system/mahi/mahi_ui_controller.h"
#include "ash/system/mahi/mahi_ui_update.h"
#include "ash/system/mahi/mahi_utils.h"
#include "ash/system/mahi/resources/grit/mahi_resources.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/animated_image_view.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// Constants -------------------------------------------------------------------

// ErrorBubble
constexpr auto kErrorBubbleInteriorMargin = gfx::Insets::TLBR(/*top=*/4,
                                                              /*left=*/4,
                                                              /*bottom=*/0,
                                                              /*right=*/8);
constexpr int kErrorIconSize = 16;
constexpr auto kErrorLabelInteriorMargin =
    gfx::Insets::TLBR(/*top=*/0, /*left=*/8, /*bottom=*/0, /*right=*/0);
constexpr int kErrorLabelMaximumWidth = 276;

// MahiQuestionAnswerView
constexpr gfx::Insets kQuestionAnswerInteriorMargin(/*all=*/8);
constexpr auto kTextBubbleInteriorMargin =
    gfx::Insets::VH(/*vertical=*/8, /*horizontal=*/12);
constexpr int kBetweenChildSpacing = 8;
constexpr int kTextBubbleCornerRadius = 12;

// TODO(b/319731776): Use panel bounds here instead of `kPanelDefaultWidth` when
// the panel is resizable.
constexpr int kTextBubbleLabelDefaultMaximumWidth =
    mahi_constants::kScrollViewWidth - kQuestionAnswerInteriorMargin.width() -
    kTextBubbleInteriorMargin.width();

// ErrorBubble -----------------------------------------------------------------

// A bubble presenting the error introduced by answering a question.
class ErrorBubble : public views::FlexLayoutView {
  METADATA_HEADER(ErrorBubble, views::FlexLayoutView)
 public:
  explicit ErrorBubble(int error_text_id) {
    views::Builder<views::FlexLayoutView>(this)
        .SetBorder(views::CreateEmptyBorder(kErrorBubbleInteriorMargin))
        .SetOrientation(views::LayoutOrientation::kHorizontal)
        .AddChildren(
            views::Builder<views::ImageView>()
                .SetID(mahi_constants::ViewId::kQuestionAnswerErrorImage)
                .SetImage(ui::ImageModel::FromVectorIcon(
                    vector_icons::kErrorIcon, cros_tokens::kCrosSysSecondary,
                    kErrorIconSize)),
            views::Builder<views::Label>()
                .SetBorder(views::CreateEmptyBorder(kErrorLabelInteriorMargin))
                .SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
                    TypographyToken::kCrosAnnotation1))
                .SetID(mahi_constants::ViewId::kQuestionAnswerErrorLabel)
                .SetMultiLine(true)
                .SetMaximumWidth(kErrorLabelMaximumWidth)
                .SetText(l10n_util::GetStringUTF16(error_text_id)))
        .BuildChildren();
  }
};

BEGIN_VIEW_BUILDER(ASH_EXPORT, ErrorBubble, views::FlexLayoutView)
END_VIEW_BUILDER

BEGIN_METADATA(ErrorBubble)
END_METADATA

// Creates a text bubble that will be populated with `text` and styled
// to be a question or answer based on `is_question`.
views::Builder<views::FlexLayoutView> CreateTextBubbleBuilder(
    const std::u16string& text,
    bool is_question) {
  return views::Builder<views::FlexLayoutView>()
      .SetInteriorMargin(kTextBubbleInteriorMargin)
      .SetBackground(views::CreateThemedRoundedRectBackground(
          is_question ? cros_tokens::kCrosSysSystemPrimaryContainer
                      : cros_tokens::kCrosSysSystemOnBase,
          gfx::RoundedCornersF(kTextBubbleCornerRadius)))
      .SetMainAxisAlignment(is_question ? views::LayoutAlignment::kEnd
                                        : views::LayoutAlignment::kStart)
      .CustomConfigure(base::BindOnce([](views::FlexLayoutView* layout) {
        layout->SetProperty(
            views::kFlexBehaviorKey,
            views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                     views::MaximumFlexSizeRule::kPreferred,
                                     /*adjust_height_for_width=*/true));
      }))
      .AddChildren(
          views::Builder<views::Label>()
              // Since every text bubble label has this ID, the view lookup will
              // only be performed from one parent above.
              .SetID(mahi_constants::ViewId::kQuestionAnswerTextBubbleLabel)
              .SetSelectable(true)
              .SetMultiLine(true)
              .CustomConfigure(base::BindOnce([](views::Label* label) {
                label->SetProperty(views::kFlexBehaviorKey,
                                   views::FlexSpecification(
                                       views::MinimumFlexSizeRule::kScaleToZero,
                                       views::MaximumFlexSizeRule::kPreferred,
                                       /*adjust_height_for_width=*/true));

                // TODO(crbug.com/40233803): Multiline label right now doesn't
                // work well with `FlexLayout`. The size constraint is not
                // passed down from the views tree in the first round of layout,
                // so we impose a maximum width constraint so that the first
                // layout handle the width and height constraint correctly.
                label->SetMaximumWidth(kTextBubbleLabelDefaultMaximumWidth);
              }))
              .SetText(text)
              .SetTooltipText(text)
              .SetHorizontalAlignment(is_question ? gfx::ALIGN_RIGHT
                                                  : gfx::ALIGN_LEFT)
              .SetEnabledColorId(
                  is_question ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                              : cros_tokens::kCrosSysOnSurface)
              .SetAutoColorReadabilityEnabled(false)
              .SetSubpixelRenderingEnabled(false)
              .SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
                  TypographyToken::kCrosBody2)));
}

// Create a row within the `MahiQuestionAnswerView`, corresponding to a question
// or an answer.
std::unique_ptr<views::View> CreateQuestionAnswerRow(const std::u16string& text,
                                                     bool is_question) {
  views::Builder<views::FlexLayoutView> row_builder =
      views::Builder<views::FlexLayoutView>().SetOrientation(
          views::LayoutOrientation::kHorizontal);

  views::Builder<views::FlexLayoutView> spacer =
      views::Builder<views::FlexLayoutView>().CustomConfigure(
          base::BindOnce([](views::FlexLayoutView* layout) {
            layout->SetProperty(views::kFlexBehaviorKey,
                                views::FlexSpecification(
                                    views::LayoutOrientation::kHorizontal,
                                    views::MinimumFlexSizeRule::kScaleToZero,
                                    views::MaximumFlexSizeRule::kUnbounded,
                                    /*adjust_height_for_width=*/true));
          }));

  if (is_question) {
    // Add a `FlexLayoutView` that is stretched the remaining space to the
    // left of the text bubble.
    return std::move(row_builder)
        .AddChildren(spacer, CreateTextBubbleBuilder(text, is_question))
        .Build();
  }

  // Add a `FlexLayoutView` that is stretched the remaining space to the right
  // of the text bubble.
  return std::move(row_builder)
      .AddChildren(CreateTextBubbleBuilder(text, is_question), spacer)
      .Build();
}

}  // namespace

}  // namespace ash

DEFINE_VIEW_BUILDER(ASH_EXPORT, ash::ErrorBubble)

namespace ash {

// MahiQuestionAnswerView::QuestionCountReporter -------------------------------

MahiQuestionAnswerView::QuestionCountReporter::QuestionCountReporter() =
    default;

MahiQuestionAnswerView::QuestionCountReporter::~QuestionCountReporter() =
    default;

void MahiQuestionAnswerView::QuestionCountReporter::IncreaseQuestionCount() {
  ++question_count_;
}

void MahiQuestionAnswerView::QuestionCountReporter::ReportDataAndReset() {
  base::UmaHistogramCounts100(
      mahi_constants::kQuestionCountPerMahiSessionHistogramName,
      question_count_);
  question_count_ = 0;
}

// MahiQuestionAnswerView ------------------------------------------------------

MahiQuestionAnswerView::MahiQuestionAnswerView(MahiUiController* ui_controller)
    : MahiUiController::Delegate(ui_controller), ui_controller_(ui_controller) {
  CHECK(ui_controller);

  SetOrientation(views::LayoutOrientation::kVertical);
  SetInteriorMargin(kQuestionAnswerInteriorMargin);
  SetIgnoreDefaultMainAxisMargins(true);
  SetCollapseMargins(true);
  SetDefault(views::kMarginsKey, gfx::Insets::VH(kBetweenChildSpacing, 0));
}

MahiQuestionAnswerView::~MahiQuestionAnswerView() {
  question_count_reporter_.ReportDataAndReset();
}

views::View* MahiQuestionAnswerView::GetView() {
  return this;
}

bool MahiQuestionAnswerView::GetViewVisibility(VisibilityState state) const {
  switch (state) {
    case VisibilityState::kQuestionAndAnswer:
      return true;
    case VisibilityState::kError:
    case VisibilityState::kSummaryAndOutlines:
      return false;
  }
}

void MahiQuestionAnswerView::OnUpdated(const MahiUiUpdate& update) {
  switch (update.type()) {
    case MahiUiUpdateType::kAnswerLoaded: {
      RemoveLoadingAnimatedImage();

      base::UmaHistogramTimes(
          mahi_constants::kAnswerLoadingTimeHistogramName,
          base::TimeTicks::Now() - answer_start_loading_time_);

      auto& answer = update.GetAnswer();

      AddChildView(CreateQuestionAnswerRow(answer, /*is_question=*/false));
      GetViewAccessibility().AnnounceText(answer);
      return;
    }
    case MahiUiUpdateType::kContentsRefreshInitiated:
      question_count_reporter_.ReportDataAndReset();
      RemoveAllChildViews();
      return;
    case MahiUiUpdateType::kErrorReceived: {
      RemoveLoadingAnimatedImage();

      // Creates `error_bubble_` if having an error.
      const MahiUiError& error = update.GetError();
      if (error.origin_state == VisibilityState::kQuestionAndAnswer) {
        AddChildView(
            views::Builder<ErrorBubble>(
                std::make_unique<ErrorBubble>(
                    mahi_utils::GetErrorStatusViewTextId(error.status)))
                .Build());
      }
      return;
    }
    case MahiUiUpdateType::kQuestionPosted: {
      question_count_reporter_.IncreaseQuestionCount();
      AddChildView(CreateQuestionAnswerRow(update.GetQuestion(),
                                           /*is_question=*/true));
      if (answer_loading_animated_image_) {
        LOG(ERROR) << "Loading animated image shouldn't be running when a "
                      "question can be asked";
        return;
      }

      auto* answer_loading_animated_image = AddChildView(
          views::Builder<views::AnimatedImageView>()
              .SetID(mahi_constants::ViewId::kAnswerLoadingAnimatedImage)
              .SetAccessibleName(l10n_util::GetStringUTF16(
                  IDS_ASH_MAHI_LOADING_ACCESSIBLE_NAME))
              .SetAnimatedImage(mahi_animation_utils::GetLottieAnimationData(
                  IDR_MAHI_LOADING_SUMMARY_ANIMATION))
              .AfterBuild(base::BindOnce([](views::AnimatedImageView* self) {
                self->Play(mahi_animation_utils::GetLottiePlaybackConfig(
                    *self->animated_image()->skottie(),
                    IDR_MAHI_LOADING_SUMMARY_ANIMATION));
              }))
              .Build());

      answer_loading_animated_image_.SetView(answer_loading_animated_image);

      answer_start_loading_time_ = base::TimeTicks::Now();

      return;
    }
    case MahiUiUpdateType::kQuestionReAsked: {
      const MahiQuestionParams& question_params =
          update.GetReAskQuestionParams();
      ui_controller_->SendQuestion(question_params.question,
                                   question_params.current_panel_content,
                                   MahiUiController::QuestionSource::kRetry);
      return;
    }
    case MahiUiUpdateType::kOutlinesLoaded:
    case MahiUiUpdateType::kQuestionAndAnswerViewNavigated:
    case MahiUiUpdateType::kRefreshAvailabilityUpdated:
    case MahiUiUpdateType::kSummaryLoaded:
    case MahiUiUpdateType::kSummaryAndOutlinesSectionNavigated:
    case MahiUiUpdateType::kSummaryAndOutlinesReloaded:
      return;
  }
}

void MahiQuestionAnswerView::RemoveLoadingAnimatedImage() {
  if (answer_loading_animated_image_) {
    RemoveChildViewT(answer_loading_animated_image_.view());
  }
}

BEGIN_METADATA(MahiQuestionAnswerView)
END_METADATA

}  // namespace ash
