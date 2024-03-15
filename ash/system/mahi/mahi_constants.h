// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_MAHI_CONSTANTS_H_
#define ASH_SYSTEM_MAHI_MAHI_CONSTANTS_H_

#include "ui/gfx/geometry/size.h"

namespace ash::mahi_constants {

// The view ids that will be used for all children views within the Mahi panel.
enum ViewId {
  kCloseButton = 1,
  kContentTitle,
  kContentIcon,
  kSummaryLabel,
  kThumbsUpButton,
  kThumbsDownButton,
  kLearnMoreLink,
  kRefreshView,
  kMahiPanelView,
  kSummaryOutlinesSection,
  kQuestionAnswerView,
  kAskQuestionSendButton,
  kBackButton,
  kOutlinesContainer,
  kSummaryLoadingAnimatedImage,
  kOutlinesLoadingAnimatedImage,
  kPanelContentsContainer,
};

// The size of the icon that appears in the panel's source row.
inline constexpr gfx::Size kContentIconSize = gfx::Size(16, 16);

inline constexpr char kMahiFeedbackHistogramName[] = "Ash.Mahi.Feedback";

// TODO(b/319264190): Replace the string here with the correct URL.
inline constexpr char kLearnMorePage[] = "https://google.com";

inline constexpr int kRefreshBannerStackDepth = 25;
inline constexpr int kPanelCornerRadius = 16;

// Delays used in `FakeMahiManager` for testing.
inline constexpr int kFakeMahiManagerLoadSummaryDelaySeconds = 4;
inline constexpr int kFakeMahiManagerLoadOutlinesDelaySeconds = 6;

}  // namespace ash::mahi_constants

#endif  // ASH_SYSTEM_MAHI_MAHI_CONSTANTS_H_
