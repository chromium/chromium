// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_MAHI_CONSTANTS_H_
#define ASH_SYSTEM_MAHI_MAHI_CONSTANTS_H_

#include "base/time/time.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/highlight_border.h"

namespace ash::mahi_constants {

// The view ids that will be used for all children views within the Mahi panel.
enum ViewId {
  kMahiPanelMainContainer = 1,
  kCloseButton,
  kContentSourceButton,
  kScrollView,
  kScrollViewContents,
  kSummaryLabel,
  kThumbsUpButton,
  kThumbsDownButton,
  kLearnMoreLink,
  kRefreshView,
  kMahiPanelView,
  kSummaryOutlinesSection,
  kQuestionAnswerView,
  kAskQuestionSendButton,
  kGoToQuestionAndAnswerButton,
  kGoToSummaryOutlinesButton,
  kOutlinesContainer,
  kSummaryLoadingAnimatedImage,
  kOutlinesLoadingAnimatedImage,
  kAnswerLoadingAnimatedImage,
  kPanelContentsContainer,
  kQuestionTextfield,
  // Since every text bubble label has this ID, the view lookup will
  // only be performed from one parent above.
  kQuestionAnswerTextBubbleLabel,
  // TODO(b/330643995): Remove this when outlines are shown by default.
  kOutlinesSectionContainer,
  kBannerTitleLabel,
  kRefreshButton,
  kErrorStatusView,
  kErrorStatusLabel,
  kErrorStatusRetryLink,
  kQuestionAnswerErrorImage,
  kQuestionAnswerErrorLabel,
  kInfoSparkIcon,
};

// The size of the icon that appears in the panel's source row.
inline constexpr gfx::Size kContentIconSize = gfx::Size(16, 16);

inline constexpr int kPanelDefaultWidth = 360;
inline constexpr int kPanelDefaultHeight = 492;
inline constexpr gfx::Insets kPanelPadding = gfx::Insets::TLBR(12, 15, 15, 15);

inline constexpr int kScrollViewWidth = kPanelDefaultWidth -
                                        views::kHighlightBorderThickness * 2 -
                                        kPanelPadding.width();

inline constexpr int kScrollContentsViewBottomPadding = 40;

inline constexpr char kLearnMorePage[] =
    "https://support.google.com/chromebook/?p=settings_help_me_read_write";

inline constexpr int kRefreshBannerStackDepth = 25;
inline constexpr int kPanelCornerRadius = 16;

// Delays used in `FakeMahiManager` for testing.
inline constexpr int kFakeMahiManagerLoadAnswerDelaySeconds = 3;
inline constexpr int kFakeMahiManagerLoadSummaryDelaySeconds = 4;
inline constexpr int kFakeMahiManagerLoadOutlinesDelaySeconds = 6;

// Nudge constants
inline constexpr char kMahiNudgeId[] = "mahi.nudge";
inline constexpr base::TimeDelta kNudgeTimeBetweenShown = base::Hours(24);
inline constexpr int kNudgeMaxShownCount = 3;

// Metrics
// Contains the types of button existed in Mahi Panel widget. Note: this should
// be kept in sync with `PanelButton` enum in
// tools/metrics/histograms/metadata/ash/enums.xml
enum class PanelButton {
  kCloseButton = 0,
  kLearnMoreLink = 1,
  kAskQuestionSendButton = 2,
  kGoToSummaryOutlinesButton = 3,
  kRefreshButton = 4,
  kGoToQuestionAndAnswerButton = 5,
  kMaxValue = kGoToQuestionAndAnswerButton,
};

inline constexpr char kMahiFeedbackHistogramName[] = "Ash.Mahi.Feedback";
inline constexpr char kMahiButtonClickHistogramName[] =
    "Ash.Mahi.ButtonClicked";
inline constexpr char kAnswerLoadingTimeHistogramName[] =
    "Ash.Mahi.QuestionAnswer.LoadingTime";
inline constexpr char kSummaryLoadingTimeHistogramName[] =
    "Ash.Mahi.Summary.LoadingTime";
inline constexpr char kMahiUserJourneyTimeHistogramName[] =
    "Ash.Mahi.UserJourneyTime";
inline constexpr char kMahiQuestionSourceHistogramName[] =
    "Ash.Mahi.QuestionSource";
inline constexpr char kQuestionCountPerMahiSessionHistogramName[] =
    "Ash.Mahi.QuestionCountPerMahiSession";
inline constexpr char kTimesMahiPanelOpenedPerSessionHistogramName[] =
    "Ash.Mahi.TimesPanelOpenedPerSession";

}  // namespace ash::mahi_constants

#endif  // ASH_SYSTEM_MAHI_MAHI_CONSTANTS_H_
