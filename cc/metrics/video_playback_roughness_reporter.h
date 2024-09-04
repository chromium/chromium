// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_VIDEO_PLAYBACK_ROUGHNESS_REPORTER_H_
#define CC_METRICS_VIDEO_PLAYBACK_ROUGHNESS_REPORTER_H_

#include <optional>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "cc/cc_export.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

// This class tracks moments when each frame was submitted
// and when it was displayed. Then series of frames split into groups
// of consecutive frames, where each group takes about one second of playback.
// Such groups also called 'frame windows'. Each windows is assigned a roughness
// score that measures how far playback smoothness was from the ideal playback.
//
// Information about several windows and their roughness score is aggregated
// for a couple of playback minutes ("measurement interval") and then a window
// with 95-percentile-max-roughness is reported via the provided callback.
//
// This sufficiently bad roughness window is deemed to represent overall
// playback quality.
class CC_EXPORT VideoPlaybackRoughnessReporter {
 public:
  struct Measurement {
    // 95%-worst window measurements ========
    // These are taken from the |kPercentileToSubmit| worst window in a
    // measurement interval.

    // |frames| - number of video frames in the window
    int frames = 0;

    // |duration| - intended wallclock duration of the window (~1s)
    base::TimeDelta duration;

    // |roughness| - roughness of the window
    double roughness = 0;

    // Per-measurement interval measurements ========
    // These are measured over all windows in the measurement interval, without
    // regard to which window was chosen above.

    // |frame_size| - size of the video frames in the window
    gfx::Size frame_size;

    // |freezing| maximum amount of time that any VideoFrame in measurement
    // interval was on-screen beyond the amount of time it should have been.
    //
    // TODO(liberato): Should this be expressed in terms of the playback rate?
    // As in, "twice as long as it should have been"?
    base::TimeDelta freezing;

    // |refresh_rate_hz| - display refresh rate, usually 60Hz
    int refresh_rate_hz = 0;
  };

  // Callback to report video playback roughness on a particularly bumpy
  // interval.
  using ReportingCallback = base::RepeatingCallback<void(const Measurement&)>;

  using TokenType = uint32_t;
  explicit VideoPlaybackRoughnessReporter(ReportingCallback reporting_cb);
  VideoPlaybackRoughnessReporter(const VideoPlaybackRoughnessReporter&) =
      delete;
  VideoPlaybackRoughnessReporter& operator=(
      const VideoPlaybackRoughnessReporter&) = delete;
  ~VideoPlaybackRoughnessReporter();
  void FrameSubmitted(TokenType token,
                      const media::VideoFrame& frame,
                      base::TimeDelta render_interval);
  void FramePresented(TokenType token,
                      base::TimeTicks timestamp,
                      bool reliable_timestamp);
  void ProcessFrameWindow();
  void Reset();

  void set_is_media_stream(bool is_media_stream) {
    is_media_stream_ = is_media_stream;
  }

  // A lower bound on how many frames can be in ConsecutiveFramesWindow
  static constexpr int kMinWindowSize = 6;

  // An upper bound on how many frames can be in ConsecutiveFramesWindow
  static constexpr int kMaxWindowSize = 60;

  // How many frame windows should be observed before reporting smoothness
  // due to playback time.
  // 1 second per window, 100 windows. It means smoothness will be reported
  // for every 100 seconds of playback.
  static constexpr int kMaxWindowsBeforeSubmit = 100;

  // How many frame windows should be observed to report soothness on last
  // time before the destruction of the reporter.
  static constexpr int kMinWindowsBeforeSubmit = kMaxWindowsBeforeSubmit / 5;

  // A frame window with this percentile of playback roughness gets reported.
  // Lower value means more tolerance to rough playback stretches.
  static constexpr int kPercentileToSubmit = 95;
  static_assert(kPercentileToSubmit > 0 && kPercentileToSubmit < 100,
                "invalid percentile value");

 private:
  friend class VideoPlaybackRoughnessReporterTest;
  struct FrameInfo {
    FrameInfo();
    FrameInfo(const FrameInfo&);
    TokenType token = 0;
    std::optional<base::TimeTicks> decode_time;
    std::optional<base::TimeTicks> presentation_time;
    std::optional<base::TimeDelta> actual_duration;
    std::optional<base::TimeDelta> intended_duration;
    int refresh_rate_hz = 60;
    gfx::Size size;
  };

  struct ConsecutiveFramesWindow {
    int size;
    base::TimeTicks first_frame_time;
    base::TimeDelta intended_duration;
    int refresh_rate_hz = 60;
    gfx::Size frame_size;

    // Root-mean-square error of the differences between the intended
    // duration and the actual duration, calculated for all subwindows
    // starting at the beginning of the smoothness window
    // [1-2][1-3][1-4] ... [1-N].
    base::TimeDelta root_mean_square_error;

    double roughness() const;

    bool operator<(const ConsecutiveFramesWindow& rhs) const {
      double r1 = roughness();
      double r2 = rhs.roughness();
      if (r1 == r2) {
        // If roughnesses are equal use window start time as a tie breaker.
        // We don't want |flat_set worst_windows_| to dedup windows with
        // the same roughness.
        return first_frame_time > rhs.first_frame_time;
      }

      // Reverse sorting order to make sure that better windows go at the
      // end of |worst_windows_| set. This way it's cheaper to remove them.
      return r1 > r2;
    }
  };

  void ReportWindow(const ConsecutiveFramesWindow& win);
  void SubmitPlaybackRoughness();

  base::circular_deque<FrameInfo> frames_;
  base::flat_set<ConsecutiveFramesWindow> worst_windows_;
  ReportingCallback reporting_cb_;
  int windows_seen_ = 0;
  int frames_window_size_ = kMinWindowSize;

  // Worst case difference between a frame's intended duration and
  // actual duration, calculated for all frames in the reporting interval.
  base::TimeDelta max_single_frame_error_;

  bool is_media_stream_ = false;
};

}  // namespace cc

#endif  // CC_METRICS_VIDEO_PLAYBACK_ROUGHNESS_REPORTER_H_
