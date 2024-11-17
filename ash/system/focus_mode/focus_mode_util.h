// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_UTIL_H_
#define ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_UTIL_H_

#include <string>

#include "ash/ash_export.h"
#include "base/time/time.h"
#include "google_apis/common/api_error_codes.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

class SystemTextfield;

namespace focus_mode_util {

enum class SoundState {
  kNone,      // The playlist is not selected and not playing.
  kSelected,  // The playlist is selected but hasn't started playing.
  kPaused,    // The playlist is selected but is paused during a focus session.
  kPlaying,   // The playlist is selected and playing during a focus session.
};

enum class SoundType {
  kNone = 0,
  kSoundscape = 1,
  kYouTubeMusic = 2,
};

struct ASH_EXPORT SelectedPlaylist {
  SelectedPlaylist();
  SelectedPlaylist(const SelectedPlaylist& other);
  SelectedPlaylist& operator=(const SelectedPlaylist& other);
  ~SelectedPlaylist();

  bool empty() const { return id.empty(); }

  std::string id;
  std::string title;
  gfx::ImageSkia thumbnail;
  focus_mode_util::SoundType type = focus_mode_util::SoundType::kNone;
  focus_mode_util::SoundState state = focus_mode_util::SoundState::kNone;
  uint8_t list_position = 0;
};

// Values for the "ash.focus_mode.focus_mode_sounds_enabled" policy.
inline constexpr char kFocusModeSoundsEnabled[] = "enabled";
inline constexpr char kFocusSoundsOnly[] = "focus-sounds";
inline constexpr char kFocusModeSoundsDisabled[] = "disabled";

constexpr std::string_view kTaskListIdKey = "taskListId";
constexpr std::string_view kTaskIdKey = "taskId";
constexpr std::string_view kSoundTypeKey = "SoundType";
constexpr std::string_view kPlaylistIdKey = "playlistId";

constexpr base::TimeDelta kMinimumDuration = base::Minutes(1);
constexpr base::TimeDelta kMaximumDuration = base::Minutes(300);

constexpr base::TimeDelta kInitialEndingMomentDuration = base::Seconds(9);

// Number of steps we break the tray circular progress indicator into.
constexpr int kProgressIndicatorSteps = 120;

// The amount of time to extend the focus session duration by during a currently
// active focus session.
constexpr base::TimeDelta kExtendDuration = base::Minutes(10);

constexpr char kFocusModeEndingMomentNudgeId[] =
    "focus_mode_ending_moment_nudge";
constexpr size_t kCongratulatoryTitleNum = 6;

// Adaptation of `base::TimeDurationFormat`. This helper function
// takes a `TimeDelta` and returns the time formatted according to
// `digital_format`.
// Passing `true` for `digital_format` returns `duration_to_format` in a numeric
//   width, excluding the hour if it is a leading zero. For example "0:30",
//   "4:30", "1:04:30".
// Passing `false` for `digital_format` returns the time in a short width
//   including hours only when nonzero and focus mode is active, minutes when
//   not a leading zero, and seconds only when `duration_to_format` is less than
//   a minute. For example when focus mode is active "30 sec", "4 min", "1 hr, 4
//   min", and when focus mode is not active "30 sec", "4 min", "64 min".
// All examples were for times of 30 seconds, 4 minutes and 30 seconds, and 1
// hour 4 minutes and 30 seconds.
// Returns a default formatted string in cases where formatting the time
// duration returns an error.
ASH_EXPORT std::u16string GetDurationString(base::TimeDelta duration_to_format,
                                            bool digital_format);

// Returns a string of `end_time` formatted with the correct clock type. For
// example: "5:10 PM" for 12-hour clock, "17:10" for 24-hour clock.
ASH_EXPORT std::u16string GetFormattedClockString(const base::Time end_time);

// Returns a string indicating that do not disturb will be turned off when the
// focus session ends at `end_time`.
ASH_EXPORT std::u16string GetNotificationDescriptionForFocusSession(
    const base::Time end_time);

// Reads the `timer_textfield`'s text and converts it to an integer.
ASH_EXPORT int GetTimerTextfieldInputInMinutes(
    SystemTextfield* timer_textfield);

// Returns a string of `end_time` formatted for the "Until" end time label. For
// example: "Until 1:00 PM".
ASH_EXPORT std::u16string GetFormattedEndTimeString(const base::Time end_time);

// Returns the desired source title string to be shown in the media controls for
// the provided playlist.
ASH_EXPORT std::string GetSourceTitleForMediaControls(
    const SelectedPlaylist& playlist);

// Returns a congratulatory text for the ending moment.
ASH_EXPORT std::u16string GetCongratulatoryText(const size_t index);

// Returns an emoji after a congratulatory text for the ending moment.
ASH_EXPORT std::u16string GetCongratulatoryEmoji(const size_t index);

// Returns a congratulatory text followed by an emoji during the ending moment.
ASH_EXPORT std::u16string GetCongratulatoryTextAndEmoji(const size_t index);

// Returns the next progress ring step the progress ring needs to equal or
// exceed to trigger a paint. This threshold is calculated by breaking down the
// entire progress into equal parts (`kProgressIndicatorSteps`), then returning
// the threshold required to hit the next step.
ASH_EXPORT int GetNextProgressStep(double current_progress);

ASH_EXPORT void RecordHistogramForApiStatus(
    const std::string& method,
    const google_apis::ApiErrorCode error_code);

ASH_EXPORT void RecordHistogramForApiLatency(const std::string& method,
                                             const base::TimeDelta latency);

ASH_EXPORT void RecordHistogramForApiResult(const std::string& method,
                                            const bool successful);

ASH_EXPORT void RecordHistogramForApiRetryCount(const std::string& method,
                                                const int retry_count);

}  // namespace focus_mode_util

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_UTIL_H_
