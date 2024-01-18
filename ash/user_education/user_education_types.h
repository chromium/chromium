// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_USER_EDUCATION_TYPES_H_
#define ASH_USER_EDUCATION_USER_EDUCATION_TYPES_H_

#include "base/containers/enum_set.h"
#include "base/memory/raw_ptr.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// Aliases ---------------------------------------------------------------------

// Type used to uniquely identify a help bubble instance.
using HelpBubbleKey = raw_ptr<const void>;

// Enums -----------------------------------------------------------------------

// Each value uniquely identifies a help bubble. Used to gate creation of new
// help bubbles to avoid spamming the user.
enum class HelpBubbleId {
  kMinValue,
  kHoldingSpaceWallpaperNudge = kMinValue,
  kTest,
  kWelcomeTourExploreApp,
  kWelcomeTourHomeButton,
  kWelcomeTourSearchBox,
  kWelcomeTourSettingsApp,
  kWelcomeTourShelf,
  kWelcomeTourStatusArea,
  kMaxValue = kWelcomeTourStatusArea,
};

// Each value uniquely identifies a style of help bubble. Help bubbles of
// different styles may differ both in terms of appearance as well as behavior.
enum class HelpBubbleStyle {
  kMinValue,
  kDialog = kMinValue,
  kNudge,
  kMaxValue = kNudge,
};

// Each value uniquely identifies a ping. Used to gate creation of new pings to
// avoid spamming the user.
enum class PingId {
  kMinValue,
  kHoldingSpaceWallpaperNudge = kMinValue,
  kTest1,
  kTest2,
  kMaxValue = kTest2,
};

// Enumeration of quantized time values for easy to interpret metrics about
// human scale time measurements that can range from minutes to weeks. These
// values are persisted to logs. Entries should not be renumbered and numeric
// values should never be reused. Be sure to update `kAllTimeBucketsSet`
// accordingly.
enum class TimeBucket {
  kMinValue = 0,
  kOneMinute = kMinValue,
  kTenMinutes = 1,
  kOneHour = 2,
  kOneDay = 3,
  kOneWeek = 4,
  kTwoWeeks = 5,
  kOverTwoWeeks = 6,
  kMaxValue = kOverTwoWeeks,
};

static constexpr auto kAllTimeBucketsSet =
    base::EnumSet<TimeBucket, TimeBucket::kMinValue, TimeBucket::kMaxValue>({
        TimeBucket::kOneMinute,
        TimeBucket::kTenMinutes,
        TimeBucket::kOneHour,
        TimeBucket::kOneDay,
        TimeBucket::kOneWeek,
        TimeBucket::kTwoWeeks,
        TimeBucket::kOverTwoWeeks,
    });

// Each value uniquely identifies a feature tutorial. Used to gate creation of
// new feature tutorials to avoid spamming the user.
enum class TutorialId {
  kMinValue,
  kTest1 = kMinValue,
  kTest2,
  kWelcomeTour,
  kMaxValue = kWelcomeTour,
};

// HelpBubbleMetadata ----------------------------------------------------------

// Type used to abstractly represent state of a single help bubble instance. The
// user education framework intentionally does not expose help bubble instances
// to clients as their implementation is private and direct interaction with
// them is not permitted.
struct HelpBubbleMetadata {
  HelpBubbleMetadata(const HelpBubbleKey key,
                     const aura::Window* anchor_root_window,
                     const gfx::Rect& anchor_bounds_in_screen);

  HelpBubbleMetadata(const HelpBubbleMetadata&) = delete;
  HelpBubbleMetadata& operator=(const HelpBubbleMetadata&) = delete;
  ~HelpBubbleMetadata();

  // Uniquely identifies a help bubble instance.
  const HelpBubbleKey key;

  // The root window associated with the help bubble instance's anchor.
  const raw_ptr<const aura::Window> anchor_root_window;

  // The bounds of the help bubble instance's anchor in screen coordinates.
  gfx::Rect anchor_bounds_in_screen;
};

}  // namespace ash

#endif  // ASH_USER_EDUCATION_USER_EDUCATION_TYPES_H_
