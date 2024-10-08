// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_panel_view.h"

#include <climits>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/url_constants.h"
#include "ash/controls/rounded_scroll_bar.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/style/style_util.h"
#include "ash/style/typography.h"
#include "ash/system/mahi/mahi_constants.h"
#include "ash/system/mahi/mahi_content_source_button.h"
#include "ash/system/mahi/mahi_error_status_view.h"
#include "ash/system/mahi/mahi_question_answer_view.h"
#include "ash/system/mahi/mahi_ui_controller.h"
#include "ash/system/mahi/mahi_ui_update.h"
#include "ash/system/mahi/mahi_utils.h"
#include "ash/system/mahi/summary_outlines_section.h"
#include "ash/wm/system_panel_view.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chromeos/components/magic_boost/public/cpp/views/experiment_badge.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/scrollbar/scroll_bar.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr int kPanelChildSpacing = 12;
constexpr int kHeaderRowSpacing = 8;

// Ask Question container constants.
constexpr gfx::Insets kAskQuestionContainerInteriorMargin =
    gfx::Insets::VH(2, 8);
constexpr int kAskQuestionContainerCornerRadius = 8;
constexpr int kInputRowContainerBetweenChildSpacing = 8;
constexpr gfx::Insets kInputTextfieldPadding = gfx::Insets::TLBR(0, 0, 0, 8);

// The below constants for the feedback buttons and cutout dimensions refer to
// the following spec, where an order is designated for the first, second, and
// third curves of the cutout in the content section's bottom-right corner:
// http://screen/9K4tXBZXihWN9KA.
constexpr int kInfoSparkIconSize = 18;
constexpr gfx::Insets kInfoSparkIconPadding = gfx::Insets::VH(0, 2);

// There's an 8px extra spacing between the scroll view and the input textfield
// (on top of the default 8px spacing for the whole panel).
constexpr int kScrollViewAndAskQuestionSpacing = 8;

constexpr int kFooterSpacing = 1;

constexpr base::TimeDelta kPanelShowAnimationDelay = base::Milliseconds(50);
constexpr base::TimeDelta kPanelShowAnimationDuration = base::Milliseconds(300);

// Sets focus ring for `question_textfield`. Insets need to be negative so it
// can exceed the textfield bounds and cover the entire container.
void InstallTextfieldFocusRing(views::View* question_textfield,
                               views::View* send_button) {
  int focus_ring_left_inset = -(kAskQuestionContainerInteriorMargin.left());
  int focus_ring_right_inset = -(kAskQuestionContainerInteriorMargin.right() +
                                 send_button->GetPreferredSize().width());
  views::FocusRing::Install(question_textfield);
  views::FocusRing::Get(question_textfield)
      ->SetColorId(ui::kColorSysStateFocusRing);
  views::InstallRoundRectHighlightPathGenerator(
      question_textfield,
      gfx::Insets::TLBR(0, focus_ring_left_inset, 0, focus_ring_right_inset),
      kAskQuestionContainerCornerRadius);
}

// FeedbackButton --------------------------------------------------------------

// Types of feedback button.
enum class FeedbackType {
  kThumbsUp,
  kThumbsDown,
};

const gfx::VectorIcon& GetFeedbackButtonIcon(FeedbackType feedback_type,
                                             bool is_toggled) {
  switch (feedback_type) {
    case FeedbackType::kThumbsUp:
      return is_toggled ? kMahiThumbsUpFilledIcon : kMahiThumbsUpIcon;
    case FeedbackType::kThumbsDown:
      return is_toggled ? kMahiThumbsDownFilledIcon : kMahiThumbsDownIcon;
  }
}

// Button with a thumbs up or thumbs down icon which users can click to provide
// feedback.
class FeedbackButton : public IconButton {
  METADATA_HEADER(FeedbackButton, IconButton)

 public:
  FeedbackButton(FeedbackType feedback_type,
                 base::RepeatingClosure on_toggle_to_active_callback)
      : IconButton(
            views::Button::PressedCallback(),
            IconButton::Type::kSmallFloating,
            &GetFeedbackButtonIcon(feedback_type, /*is_toggled=*/false),
            feedback_type == FeedbackType::kThumbsUp
                ? IDS_ASH_MAHI_THUMBS_UP_FEEDBACK_BUTTON_ACCESSIBLE_NAME
                : IDS_ASH_MAHI_THUMBS_DOWN_FEEDBACK_BUTTON_ACCESSIBLE_NAME,
            /*is_togglable=*/true,
            /*has_border=*/false),
        feedback_type_(feedback_type),
        on_toggle_to_active_callback_(std::move(on_toggle_to_active_callback)) {
    SetCallback(base::BindRepeating(&FeedbackButton::OnButtonPressed,
                                    weak_ptr_factory_.GetWeakPtr()));

    SetToggledVectorIcon(
        GetFeedbackButtonIcon(feedback_type, /*is_toggled=*/true));
    SetIconColor(cros_tokens::kCrosSysSecondary);
    SetIconToggledColor(cros_tokens::kCrosSysSecondary);
    SetBackgroundColor(SK_ColorTRANSPARENT);
    SetBackgroundToggledColor(SK_ColorTRANSPARENT);
  }
  FeedbackButton(const FeedbackButton&) = delete;
  FeedbackButton& operator=(const FeedbackButton&) = delete;
  ~FeedbackButton() override = default;

 private:
  void OnButtonPressed() {
    SetToggled(!toggled());
    if (!toggled()) {
      return;
    }

    on_toggle_to_active_callback_.Run();
    base::UmaHistogramBoolean(mahi_constants::kMahiFeedbackHistogramName,
                              feedback_type_ == FeedbackType::kThumbsUp);

    // Open the feedback dialog when thumbs down is pressed, but only if it
    // hasn't already been shown previously.
    if (feedback_type_ == FeedbackType::kThumbsDown &&
        !feedback_dialog_was_shown_) {
      if (auto* const manager = chromeos::MahiManager::Get()) {
        manager->OpenFeedbackDialog();
        feedback_dialog_was_shown_ = true;
      } else {
        CHECK_IS_TEST();
      }
    }
  }

  const FeedbackType feedback_type_;

  // Whether the feedback dialog has been shown.
  bool feedback_dialog_was_shown_ = false;

  // Callback when this button is toggled from not toggled to toggled.
  base::RepeatingClosure on_toggle_to_active_callback_;

  base::WeakPtrFactory<FeedbackButton> weak_ptr_factory_{this};
};

BEGIN_VIEW_BUILDER(/*no export*/, FeedbackButton, views::View)
END_VIEW_BUILDER

BEGIN_METADATA(FeedbackButton)
END_METADATA

// GoToSummaryOutlinesButton --------------------------------------------------
// A button used to navigate to the summary & outlines section. Only shown when
// in the Q&A view.
class GoToSummaryOutlinesButton : public IconButton,
                                  public MahiUiController::Delegate {
  METADATA_HEADER(GoToSummaryOutlinesButton, IconButton)

 public:
  // NOTE: `controller` outlives `GoToSummaryOutlinesButton` so it is safe to
  // use `base::Unretained()` here.
  explicit GoToSummaryOutlinesButton(MahiUiController* ui_controller)
      : IconButton(
            /*callback=*/base::BindRepeating(
                [](MahiUiController* ui_controller) {
                  ui_controller->NavigateToSummaryOutlinesSection();

                  base::UmaHistogramEnumeration(
                      mahi_constants::kMahiButtonClickHistogramName,
                      mahi_constants::PanelButton::kGoToSummaryOutlinesButton);
                },
                base::Unretained(ui_controller)),
            IconButton::Type::kSmallFloating,
            &kEcheArrowBackIcon,
            /*accessible_name=*/
            l10n_util::GetStringUTF16(
                IDS_ASH_MAHI_PANEL_GO_TO_SUMMARY_BUTTON_ACCESSIBLE_NAME),
            /*is_togglable=*/false,
            /*has_border=*/false),
        MahiUiController::Delegate(ui_controller) {}

  GoToSummaryOutlinesButton(const GoToSummaryOutlinesButton&) = delete;
  GoToSummaryOutlinesButton& operator=(const GoToSummaryOutlinesButton&) =
      delete;
  ~GoToSummaryOutlinesButton() override = default;

 private:
  // MahiController::Delegate:
  views::View* GetView() override { return this; }

  bool GetViewVisibility(VisibilityState state) const override {
    switch (state) {
      case VisibilityState::kError:
        return GetVisible();
      case VisibilityState::kQuestionAndAnswer:
        return true;
      case VisibilityState::kSummaryAndOutlines:
        return false;
    }
  }
};

BEGIN_METADATA(GoToSummaryOutlinesButton)
END_METADATA

BEGIN_VIEW_BUILDER(/*no export*/, GoToSummaryOutlinesButton, views::View)
END_VIEW_BUILDER

// GoToQuestionAndAnswerButton ------------------------------------------------
// A button used to navigate back to the Q&A view from the summary and outlines
// section. Shown only if the Q&A view has contents.
class GoToQuestionAndAnswerButton : public IconButton,
                                    public MahiUiController::Delegate {
  METADATA_HEADER(GoToQuestionAndAnswerButton, IconButton)

 public:
  // NOTE: `controller` outlives `GoToQuestionAndAnswerButton` so it is safe to
  // use `base::Unretained()` here.
  explicit GoToQuestionAndAnswerButton(MahiUiController* ui_controller)
      : IconButton(
            /*callback=*/base::BindRepeating(
                [](MahiUiController* ui_controller) {
                  ui_controller->NavigateToQuestionAnswerView();

                  base::UmaHistogramEnumeration(
                      mahi_constants::kMahiButtonClickHistogramName,
                      mahi_constants::PanelButton::
                          kGoToQuestionAndAnswerButton);
                },
                base::Unretained(ui_controller)),
            IconButton::Type::kSmallFloating,
            &kQuickSettingsRightArrowIcon,
            /*accessible_name=*/
            l10n_util::GetStringUTF16(
                IDS_ASH_MAHI_PANEL_GO_TO_QUESTION_AND_ANSWER_BUTTON_ACCESSIBLE_NAME),
            /*is_togglable=*/false,
            /*has_border=*/false),
        MahiUiController::Delegate(ui_controller) {}

  GoToQuestionAndAnswerButton(const GoToQuestionAndAnswerButton&) = delete;
  GoToQuestionAndAnswerButton& operator=(const GoToQuestionAndAnswerButton&) =
      delete;
  ~GoToQuestionAndAnswerButton() override = default;

 private:
  // MahiController::Delegate:
  views::View* GetView() override { return this; }

  bool GetViewVisibility(VisibilityState state) const override {
    switch (state) {
      case VisibilityState::kError:
        return GetVisible();
      case VisibilityState::kQuestionAndAnswer:
        return false;
      case VisibilityState::kSummaryAndOutlines:
        return question_answer_view_has_contents_;
    }
  }

  void OnUpdated(const MahiUiUpdate& update) override {
    switch (update.type()) {
      case MahiUiUpdateType::kContentsRefreshInitiated:
        SetVisible(false);
        question_answer_view_has_contents_ = false;
        break;
      case MahiUiUpdateType::kQuestionPosted:
        // We do not show the button immediately because we navigate to the Q&A
        // view when a question is posted.
        question_answer_view_has_contents_ = true;
        break;
      case MahiUiUpdateType::kAnswerLoaded:
      case MahiUiUpdateType::kErrorReceived:
      case MahiUiUpdateType::kOutlinesLoaded:
      case MahiUiUpdateType::kQuestionAndAnswerViewNavigated:
      case MahiUiUpdateType::kQuestionReAsked:
      case MahiUiUpdateType::kRefreshAvailabilityUpdated:
      case MahiUiUpdateType::kSummaryAndOutlinesReloaded:
      case MahiUiUpdateType::kSummaryAndOutlinesSectionNavigated:
      case MahiUiUpdateType::kSummaryLoaded:
        return;
    }
  }

  bool question_answer_view_has_contents_ = false;
};

BEGIN_METADATA(GoToQuestionAndAnswerButton)
END_METADATA

BEGIN_VIEW_BUILDER(/*no export*/, GoToQuestionAndAnswerButton, views::View)
END_VIEW_BUILDER

// MahiScrollView -----------------------------------------------------------

// Container for scrollable content in the Mahi panel, including the summary and
// outlines section or the Q&A section. Clips its own bounds to present its
// contents within a round-cornered container with a cutout in the bottom-right.
class MahiScrollView : public views::ScrollView,
                       public views::ViewTargeterDelegate,
                       public MahiUiController::Delegate {
  METADATA_HEADER(MahiScrollView, views::ScrollView)

 public:
  explicit MahiScrollView(MahiUiController* ui_controller)
      : MahiUiController::Delegate(ui_controller) {
    SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
    SetBackgroundThemeColorId(cros_tokens::kCrosSysSystemOnBase);
    ClipHeightTo(/*min_height=*/0, /*max_height=*/INT_MAX);
    SetDrawOverflowIndicator(false);
    auto scroll_bar = std::make_unique<RoundedScrollBar>(
        RoundedScrollBar::Orientation::kVertical);

    if (mahi_utils::ShouldShowFeedbackButton()) {
      // Prevent the scroll bar from overlapping with any rounded corners or
      // extending into the cutout region.
      scroll_bar->SetInsets(gfx::Insets::TLBR(
          mahi_constants::kContentScrollViewCornerRadius, 0,
          mahi_constants::kCutoutHeight + mahi_constants::kCutoutConvexRadius,
          0));
    }

    scroll_bar->SetSnapBackOnDragOutside(false);
    SetVerticalScrollBar(std::move(scroll_bar));
  }

 private:
  // views::ScrollView:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override {
    SetClipPath(mahi_utils::GetCutoutClipPath(
        /*contents_size=*/GetContentsBounds().size()));
  }

  // views::View
  void Layout(PassKey) override {
    LayoutSuperclass<views::ScrollView>(this);

    // Every time the scroll view is laid `default_scroll_position` is checked
    // to scroll to the top or bottom of the view based on the following:
    // 1. Scroll to bottom when switching to the Q&A view, or when a new
    //    question/answer is added.
    // 2. Scroll to top when switching to the summary & outlines section, or
    //    when refreshing the summary contents.
    if (default_scroll_position_.has_value()) {
      vertical_scroll_bar()->ScrollByAmount(default_scroll_position_.value());
      default_scroll_position_ = std::nullopt;
    }
  }

  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override {
    if (!mahi_utils::ShouldShowFeedbackButton()) {
      return views::ViewTargeterDelegate::DoesIntersectRect(target, rect);
    }

    return !rect.Intersects(mahi_utils::GetCornerCutoutRegion(
        /*contents_bounds=*/GetContentsBounds()));
  }

  // MahiController::Delegate:
  views::View* GetView() override { return this; }

  bool GetViewVisibility(VisibilityState state) const override {
    return GetVisible();
  }

  void OnUpdated(const MahiUiUpdate& update) override {
    views::ScrollBar::ScrollAmount old_scroll_position;
    switch (update.type()) {
      case MahiUiUpdateType::kAnswerLoaded:
      case MahiUiUpdateType::kQuestionPosted: {
        old_scroll_position = default_scroll_position_.value_or(
            views::ScrollBar::ScrollAmount::kNone);
        default_scroll_position_ = views::ScrollBar::ScrollAmount::kEnd;
        break;
      }
      case MahiUiUpdateType::kSummaryAndOutlinesSectionNavigated: {
        old_scroll_position = default_scroll_position_.value_or(
            views::ScrollBar::ScrollAmount::kNone);
        default_scroll_position_ = views::ScrollBar::ScrollAmount::kStart;
        break;
      }
      case MahiUiUpdateType::kContentsRefreshInitiated:
      case MahiUiUpdateType::kErrorReceived:
      case MahiUiUpdateType::kOutlinesLoaded:
      case MahiUiUpdateType::kQuestionAndAnswerViewNavigated:
      case MahiUiUpdateType::kQuestionReAsked:
      case MahiUiUpdateType::kRefreshAvailabilityUpdated:
      case MahiUiUpdateType::kSummaryAndOutlinesReloaded:
      case MahiUiUpdateType::kSummaryLoaded:
        break;
    }
    if (old_scroll_position != default_scroll_position_) {
      InvalidateLayout();
    }
  }

  // Used to scroll the view to a default position based on the current state.
  std::optional<views::ScrollBar::ScrollAmount> default_scroll_position_;
};

BEGIN_METADATA(MahiScrollView)
END_METADATA

views::StyledLabel::RangeStyleInfo GetLinkTextStyle(
    base::RepeatingClosure press_link_callback) {
  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(
          std::move(press_link_callback));
  link_style.override_color_id =
      static_cast<ui::ColorId>(cros_tokens::kCrosSysOnSurfaceVariant);
  return link_style;
}

}  // namespace

}  // namespace ash

DEFINE_VIEW_BUILDER(/*no export*/, ash::FeedbackButton)
DEFINE_VIEW_BUILDER(/*no export*/, ash::GoToSummaryOutlinesButton)
DEFINE_VIEW_BUILDER(/*no export*/, ash::GoToQuestionAndAnswerButton)

namespace ash {

MahiPanelView::MahiPanelView(MahiUiController* ui_controller)
    : MahiUiController::Delegate(ui_controller),
      ui_controller_(ui_controller),
      open_time_(base::TimeTicks::Now()) {
  CHECK(ui_controller_);

  SetID(mahi_constants::ViewId::kMahiPanelView);
  SetUseDefaultFillLayout(true);
  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemBaseElevated,
      mahi_constants::kPanelCornerRadius));

  // Create a layer for the view for background blur and rounded corners.
  SetPaintToLayer();
  layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF{mahi_constants::kPanelCornerRadius});
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetIsFastRoundedCorner(true);
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);
  SetBorder(std::make_unique<views::HighlightBorder>(
      mahi_constants::kPanelCornerRadius,
      views::HighlightBorder::Type::kHighlightBorderOnShadow,
      /*insets_type=*/views::HighlightBorder::InsetsType::kHalfInsets));

  // The `main_container` is used to anchor the contents to the middle of the
  // panel when its size is animating. The anchoring to middle effect is
  // achieved by setting a transform on the `main_container`s layer and
  // animating it.
  main_container_ =
      AddChildView(views::Builder<views::FlexLayoutView>()
                       .SetID(mahi_constants::ViewId::kMahiPanelMainContainer)
                       .SetOrientation(views::LayoutOrientation::kVertical)
                       .SetMainAxisAlignment(views::LayoutAlignment::kStart)
                       .SetInteriorMargin(mahi_constants::kPanelPadding)
                       .SetIgnoreDefaultMainAxisMargins(true)
                       .SetCollapseMargins(true)
                       .SetPaintToLayer()
                       .Build());
  main_container_->layer()->SetFillsBoundsOpaquely(false);
  main_container_->SetDefault(views::kMarginsKey,
                              gfx::Insets::VH(kPanelChildSpacing, 0));
  main_container_->AddChildView(CreateHeaderRow());

  // Add a button which shows the content source icon and title.
  main_container_->AddChildView(
      views::Builder<MahiContentSourceButton>()
          .CopyAddressTo(&content_source_button_)
          .SetID(mahi_constants::ViewId::kContentSourceButton)
          .Build());

  views::View* feedback_buttons_container;
  views::View* error_status_view;

  // Add a scrollable view of the panel's content, with a feedback section.
  main_container_->AddChildView(
      views::Builder<views::View>()
          .SetID(mahi_constants::ViewId::kPanelContentsContainer)
          .SetUseDefaultFillLayout(true)
          // Extra spacing between the scroll view and the input textfield.
          .SetBorder(views::CreateEmptyBorder(
              gfx::Insets::TLBR(0, 0, kScrollViewAndAskQuestionSpacing, 0)))
          .SetProperty(
              views::kFlexBehaviorKey,
              views::FlexSpecification(views::LayoutOrientation::kVertical,
                                       views::MinimumFlexSizeRule::kScaleToZero,
                                       views::MaximumFlexSizeRule::kUnbounded))
          .AddChildren(
              // Add buttons for the user to give feedback on the content.
              views::Builder<views::BoxLayoutView>()
                  .CopyAddressTo(&feedback_buttons_container)
                  .SetID(mahi_constants::ViewId::kFeedbackButtonsContainer)
                  .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
                  .SetInsideBorderInsets(gfx::Insets::TLBR(
                      0, 0, 0, mahi_constants::kFeedbackButtonIconPaddingRight))
                  .SetMainAxisAlignment(
                      views::BoxLayout::MainAxisAlignment::kEnd)
                  .SetCrossAxisAlignment(
                      views::BoxLayout::CrossAxisAlignment::kEnd)
                  .SetBetweenChildSpacing(
                      mahi_constants::kFeedbackButtonSpacing)
                  .AddChildren(
                      views::Builder<FeedbackButton>(
                          std::make_unique<FeedbackButton>(
                              FeedbackType::kThumbsUp,
                              base::BindRepeating(
                                  &MahiPanelView::OnThumbsUpButtonActive,
                                  weak_ptr_factory_.GetWeakPtr())))
                          .CopyAddressTo(&thumbs_up_button_)
                          .SetID(mahi_constants::ViewId::kThumbsUpButton),
                      views::Builder<FeedbackButton>(
                          std::make_unique<FeedbackButton>(
                              FeedbackType::kThumbsDown,
                              base::BindRepeating(
                                  &MahiPanelView::OnThumbsDownButtonActive,
                                  weak_ptr_factory_.GetWeakPtr())))
                          .CopyAddressTo(&thumbs_down_button_)
                          .SetID(mahi_constants::ViewId::kThumbsDownButton)),
              views::Builder<views::ScrollView>(
                  std::make_unique<MahiScrollView>(ui_controller_))
                  .SetID(mahi_constants::ViewId::kScrollView)
                  .SetContents(
                      views::Builder<views::FlexLayoutView>()
                          .SetID(mahi_constants::ViewId::kScrollViewContents)
                          // Extra bottom padding for http://b/332766742.
                          .SetInteriorMargin(gfx::Insets::TLBR(
                              0, 0,
                              mahi_constants::kScrollContentsViewBottomPadding,
                              0))
                          .AddChildren(
                              views::Builder<SummaryOutlinesSection>(
                                  std::make_unique<SummaryOutlinesSection>(
                                      ui_controller_))
                                  .SetID(mahi_constants::ViewId::
                                             kSummaryOutlinesSection)
                                  .CopyAddressTo(&summary_outlines_section_),
                              views::Builder<MahiQuestionAnswerView>(
                                  std::make_unique<MahiQuestionAnswerView>(
                                      ui_controller_))
                                  .SetID(mahi_constants::ViewId::
                                             kQuestionAnswerView)
                                  .CopyAddressTo(&question_answer_view_)
                                  .SetProperty(
                                      views::kFlexBehaviorKey,
                                      views::FlexSpecification(
                                          views::LayoutOrientation::kHorizontal,
                                          views::MinimumFlexSizeRule::
                                              kScaleToZero,
                                          views::MaximumFlexSizeRule::
                                              kUnbounded)))),
              views::Builder<MahiErrorStatusView>(
                  std::make_unique<MahiErrorStatusView>(ui_controller_))
                  .CopyAddressTo(&error_status_view))
          .Build());

  if (mahi_utils::ShouldShowFeedbackButton()) {
    // Put feedback buttons container after the error status view in the focus
    // list since the order of traversal should be scroll view -> error status
    // view -> feedback buttons.
    feedback_buttons_container->InsertAfterInFocusList(error_status_view);
  } else {
    feedback_buttons_container->SetVisible(false);
  }

  // Add a row for processing user input that includes a textfield, send button
  // and a back to Q&A button when in the summary section.
  views::FlexLayoutView* ask_question_container = nullptr;
  main_container_->AddChildView(
      views::Builder<views::FlexLayoutView>()
          .CustomConfigure(base::BindOnce([](views::FlexLayoutView* layout) {
            layout->SetDefault(
                views::kMarginsKey,
                gfx::Insets::VH(0, kInputRowContainerBetweenChildSpacing));
          }))
          .SetCollapseMargins(true)
          .SetIgnoreDefaultMainAxisMargins(true)
          .AddChildren(
              views::Builder<views::FlexLayoutView>()
                  .CopyAddressTo(&ask_question_container)
                  .SetBackground(views::CreateThemedRoundedRectBackground(
                      cros_tokens::kCrosSysSystemOnBase,
                      gfx::RoundedCornersF(kAskQuestionContainerCornerRadius)))
                  .SetInteriorMargin(kAskQuestionContainerInteriorMargin)
                  .SetProperty(
                      views::kFlexBehaviorKey,
                      views::FlexSpecification(views::FlexSpecification(
                          views::LayoutOrientation::kHorizontal,
                          views::MinimumFlexSizeRule::kPreferred,
                          views::MaximumFlexSizeRule::kUnbounded)))
                  .AddChildren(
                      views::Builder<views::Textfield>()
                          .CopyAddressTo(&question_textfield_)
                          .SetID(mahi_constants::ViewId::kQuestionTextfield)
                          .SetBackgroundEnabled(false)
                          .SetBorder(
                              views::CreateEmptyBorder(kInputTextfieldPadding))
                          .SetPlaceholderText(l10n_util::GetStringUTF16(
                              IDS_ASH_MAHI_PANEL_INPUT_PLACEHOLDER_TEXT))
                          .SetAccessibleName(l10n_util::GetStringUTF16(
                              IDS_ASH_MAHI_PANEL_INPUT_PLACEHOLDER_TEXT))
                          .SetFontList(
                              TypographyProvider::Get()->ResolveTypographyToken(
                                  TypographyToken::kCrosAnnotation1))
                          .SetProperty(
                              views::kFlexBehaviorKey,
                              views::FlexSpecification(views::FlexSpecification(
                                  views::LayoutOrientation::kHorizontal,
                                  views::MinimumFlexSizeRule::kPreferred,
                                  views::MaximumFlexSizeRule::kUnbounded)))
                          .SetController(this)),
              views::Builder<GoToQuestionAndAnswerButton>(
                  std::make_unique<GoToQuestionAndAnswerButton>(ui_controller_))
                  .SetID(mahi_constants::ViewId::kGoToQuestionAndAnswerButton))
          .Build());

  send_button_ = ask_question_container->AddChildView(
      IconButton::Builder()
          .SetViewId(mahi_constants::ViewId::kAskQuestionSendButton)
          .SetType(IconButton::Type::kSmallFloating)
          .SetBackgroundColor(cros_tokens::kCrosSysSystemOnBase1)
          .SetVectorIcon(&vector_icons::kSendIcon)
          .SetCallback(base::BindRepeating(&MahiPanelView::OnSendButtonPressed,
                                           weak_ptr_factory_.GetWeakPtr()))
          .SetAccessibleName(l10n_util::GetStringUTF16(
              IDS_ASH_MAHI_PANEL_INPUT_TEXTFIELD_SEND_BUTTON_ACCESSIBLE_NAME))
          .Build());

  question_textfield_->RemoveHoverEffect();
  InstallTextfieldFocusRing(question_textfield_, send_button_);

  std::unique_ptr<views::View> footer_view;
  if (mahi_utils::ShouldShowFeedbackButton()) {
    footer_view =
        views::Builder<views::BoxLayoutView>()
            .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
            .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter)
            .SetBetweenChildSpacing(kFooterSpacing)
            .AddChildren(views::Builder<views::Label>()
                             .SetID(mahi_constants::ViewId::kFooterLabel)
                             .SetText(l10n_util::GetStringUTF16(
                                 IDS_ASH_MAHI_PANEL_DISCLAIMER)),
                         views::Builder<views::Link>()
                             .SetText(l10n_util::GetStringUTF16(
                                 IDS_ASH_MAHI_LEARN_MORE_LINK_LABEL_TEXT))
                             .SetAccessibleName(l10n_util::GetStringUTF16(
                                 IDS_ASH_MAHI_LEARN_MORE_LINK_ACCESSIBLE_NAME))
                             .SetCallback(base::BindRepeating(
                                 &MahiPanelView::OnLearnMoreLinkClicked,
                                 weak_ptr_factory_.GetWeakPtr()))
                             .SetID(mahi_constants::ViewId::kLearnMoreLink))
            .Build();
  } else {
    // Use `views::StyledLabel` here instead so that the learn more link can be
    // displayed in the same row as the multilined footer text.
    std::vector<size_t> offsets;
    const std::u16string link_text =
        l10n_util::GetStringUTF16(IDS_ASH_MAHI_LEARN_MORE_LINK_LABEL_TEXT);
    const std::u16string footer_text = l10n_util::GetStringFUTF16(
        IDS_ASH_MAHI_PANEL_DISCLAIMER_FEEDBACK_DISABLED, {link_text}, &offsets);
    footer_view =
        views::Builder<views::StyledLabel>()
            .SetID(mahi_constants::ViewId::kFooterLabel)
            .SetText(footer_text)
            .AddStyleRange(
                gfx::Range(offsets.at(0), offsets.at(0) + link_text.length()),
                GetLinkTextStyle(
                    base::BindRepeating(&MahiPanelView::OnLearnMoreLinkClicked,
                                        weak_ptr_factory_.GetWeakPtr())))
            .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER)
            .SetAutoColorReadabilityEnabled(false)
            .Build();
  }

  main_container_->AddChildView(std::move(footer_view));

  // Refresh contents after all child views are built.
  ui_controller_->RefreshContents();
}

MahiPanelView::~MahiPanelView() {
  // The difference between `Now()` and `open_time_` is the duration that this
  // view has been shown.
  base::UmaHistogramLongTimes(mahi_constants::kMahiUserJourneyTimeHistogramName,
                              base::TimeTicks::Now() - open_time_);
}

void MahiPanelView::AnimatePopIn(const gfx::Rect& start_bounds) {
  gfx::Rect end_bounds = GetBoundsInScreen();

  // Add a clip rect to reduce the panel's visible area to the provided
  // `start_bounds`.
  auto clip_rect = start_bounds - end_bounds.OffsetFromOrigin();
  layer()->SetClipRect(clip_rect);

  // Anchor the panel's contents to the center of the visible area.
  auto y_offset = start_bounds.CenterPoint().y() -
                  main_container_->GetBoundsInScreen().CenterPoint().y();
  gfx::Transform transform;
  transform.Translate(gfx::Vector2d(0, y_offset));
  main_container_->layer()->SetTransform(transform);

  views::AnimationBuilder()
      .Once()
      .At(kPanelShowAnimationDelay)
      .SetDuration(kPanelShowAnimationDuration)
      .SetTransform(main_container_, gfx::Transform(),
                    gfx::Tween::ACCEL_40_DECEL_100_3)
      .SetClipRect(this, gfx::Rect(gfx::Point(), end_bounds.size()),
                   gfx::Tween::ACCEL_40_DECEL_100_3);
}

std::unique_ptr<views::View> MahiPanelView::CreateHeaderRow() {
  return views::Builder<views::FlexLayoutView>()
      .SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetIgnoreDefaultMainAxisMargins(true)
      .SetCollapseMargins(true)
      .CustomConfigure(base::BindOnce([](views::FlexLayoutView* layout) {
        layout->SetDefault(views::kMarginsKey,
                           gfx::Insets::VH(0, kHeaderRowSpacing));
      }))
      .AddChildren(
          // Back Button.
          views::Builder<GoToSummaryOutlinesButton>(
              std::make_unique<GoToSummaryOutlinesButton>(ui_controller_))
              .SetID(mahi_constants::ViewId::kGoToSummaryOutlinesButton),
          views::Builder<views::ImageView>()
              .SetID(mahi_constants::ViewId::kInfoSparkIcon)
              .SetImage(ui::ImageModel::FromVectorIcon(chromeos::kInfoSparkIcon,
                                                       ui::kColorMenuIcon,
                                                       kInfoSparkIconSize))
              .SetBorder(views::CreateEmptyBorder(kInfoSparkIconPadding)),
          // The Panel's title label
          views::Builder<views::Label>()
              .SetText(l10n_util::GetStringUTF16(IDS_ASH_MAHI_PANEL_TITLE))
              .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
              .SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
                  TypographyToken::kCrosButton1))
              .SetEnabledColorId(cros_tokens::kCrosSysOnSurface),
          // Experimental badge
          views::Builder<views::View>(
              std::make_unique<chromeos::ExperimentBadge>())
              .CustomConfigure(base::BindOnce([](views::View* view) {
                static_cast<chromeos::ExperimentBadge*>(view)
                    ->label()
                    ->SetFontList(
                        ash::TypographyProvider::Get()->ResolveTypographyToken(
                            ash::TypographyToken::kCrosLabel1));
              })),
          // Close Button, aligned to the right by setting a `FlexSpecification`
          // with unbounded maximum flex size and `LayoutAlignment::kEnd`.
          views::Builder<views::Button>(
              IconButton::Builder()
                  .SetType(IconButton::Type::kMediumFloating)
                  .SetVectorIcon(&kMediumOrLargeCloseButtonIcon)
                  .SetAccessibleName(l10n_util::GetStringUTF16(
                      IDS_ASH_MAHI_PANEL_CLOSE_BUTTON_ACCESSIBLE_NAME))
                  .Build())
              .SetID(mahi_constants::ViewId::kCloseButton)
              .SetCallback(
                  base::BindRepeating(&MahiPanelView::OnCloseButtonPressed,
                                      weak_ptr_factory_.GetWeakPtr()))
              .SetProperty(views::kFlexBehaviorKey,
                           views::FlexSpecification(
                               views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded)
                               .WithAlignment(views::LayoutAlignment::kEnd)))
      .Build();
}

bool MahiPanelView::HandleKeyEvent(views::Textfield* textfield,
                                   const ui::KeyEvent& key_event) {
  if (key_event.type() != ui::EventType::kKeyPressed) {
    return false;
  }

  if (key_event.key_code() == ui::KeyboardCode::VKEY_RETURN) {
    OnSendButtonPressed();
    return true;
  }

  return false;
}

views::View* MahiPanelView::GetView() {
  return this;
}

bool MahiPanelView::GetViewVisibility(VisibilityState state) const {
  // Do not change visibility for `state`.
  return GetVisible();
}

void MahiPanelView::OnUpdated(const MahiUiUpdate& update) {
  switch (update.type()) {
    case MahiUiUpdateType::kAnswerLoaded:
      // Input is re-enabled after backend has finished processing a question.
      send_button_->SetEnabled(true);
      return;
    case MahiUiUpdateType::kContentsRefreshInitiated: {
      content_source_button_->RefreshContentSourceInfo();

      // Reset feedback buttons when new content is requested.
      thumbs_up_button_->SetToggled(false);
      thumbs_down_button_->SetToggled(false);
      return;
    }
    case MahiUiUpdateType::kErrorReceived:
      // Input is re-enabled after backend returns an error.
      send_button_->SetEnabled(true);
      return;
    case MahiUiUpdateType::kOutlinesLoaded:
    case MahiUiUpdateType::kQuestionAndAnswerViewNavigated:
    case MahiUiUpdateType::kQuestionPosted:
    case MahiUiUpdateType::kQuestionReAsked:
    case MahiUiUpdateType::kRefreshAvailabilityUpdated:
    case MahiUiUpdateType::kSummaryLoaded:
    case MahiUiUpdateType::kSummaryAndOutlinesSectionNavigated:
    case MahiUiUpdateType::kSummaryAndOutlinesReloaded:
      return;
  }
}

void MahiPanelView::OnCloseButtonPressed(const ui::Event& event) {
  CHECK(GetWidget());
  GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
  base::UmaHistogramEnumeration(mahi_constants::kMahiButtonClickHistogramName,
                                mahi_constants::PanelButton::kCloseButton);
}

void MahiPanelView::OnLearnMoreLinkClicked() {
  NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(chrome::kHelpMeReadWriteLearnMoreURL),
      NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
  base::UmaHistogramEnumeration(mahi_constants::kMahiButtonClickHistogramName,
                                mahi_constants::PanelButton::kLearnMoreLink);
  GetViewAccessibility().AnnounceText((l10n_util::GetStringUTF16(
      IDS_ASH_MAHI_LEARN_MORE_CLICK_ACTIVATION_ACCESSIBLE_NAME)));
}

void MahiPanelView::OnSendButtonPressed() {
  // We need to check state since this function can also be called by pressing
  // the `Enter` key while the textfield is focused.
  if (!send_button_->GetEnabled()) {
    return;
  }

  // Do not process question if input is invalid.
  if (std::u16string_view trimmed_text = base::TrimWhitespace(
          question_textfield_->GetText(), base::TrimPositions::TRIM_ALL);
      !trimmed_text.empty()) {
    // Input is disabled while backend is processing a question.
    send_button_->SetEnabled(false);

    ui_controller_->SendQuestion(std::u16string(trimmed_text),
                                 /*current_panel_content=*/true,
                                 MahiUiController::QuestionSource::kPanel);
    question_textfield_->SetText(std::u16string());
    base::UmaHistogramEnumeration(
        mahi_constants::kMahiButtonClickHistogramName,
        mahi_constants::PanelButton::kAskQuestionSendButton);
  }
}

void MahiPanelView::OnThumbsUpButtonActive() {
  if (thumbs_down_button_->toggled()) {
    thumbs_down_button_->SetToggled(false);
  }
}

void MahiPanelView::OnThumbsDownButtonActive() {
  if (thumbs_up_button_->toggled()) {
    thumbs_up_button_->SetToggled(false);
  }
}

BEGIN_METADATA(MahiPanelView)
END_METADATA

}  // namespace ash
