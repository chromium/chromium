// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/video_playback_roughness_reporter.h"

#include <algorithm>

#include "base/containers/adapters.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"

namespace {
constexpr int max_worst_windows_size() {
  constexpr int size =
      1 + cc::VideoPlaybackRoughnessReporter::kMaxWindowsBeforeSubmit *
              (100 - cc::VideoPlaybackRoughnessReporter::kPercentileToSubmit) /
              100;
  static_assert(size > 1, "worst_windows_ is too small");
  static_assert(size < 25, "worst_windows_ is too big");
  return size;
}

void RecordUmaVideoFrameSubmitter(bool is_media_stream,
                                  base::TimeDelta time_since_decode) {
  if (is_media_stream) {
    base::UmaHistogramTimes("Media.VideoFrameSubmitter.Rtc.PresentationDelay",
                            time_since_decode);
  } else {
    base::UmaHistogramTimes("Media.VideoFrameSubmitter.Video.PresentationDelay",
                            time_since_decode);
  }

  // TODO(crbug.com/364352012): This will be removed once expired, kept for now
  // due to internal dependencies.
  base::UmaHistogramTimes("Media.VideoFrameSubmitter", time_since_decode);
}

}  // namespace

namespace cc {

constexpr int VideoPlaybackRoughnessReporter::kMinWindowSize;
constexpr int VideoPlaybackRoughnessReporter::kMaxWindowSize;
constexpr int VideoPlaybackRoughnessReporter::kMaxWindowsBeforeSubmit;
constexpr int VideoPlaybackRoughnessReporter::kMinWindowsBeforeSubmit;
constexpr int VideoPlaybackRoughnessReporter::kPercentileToSubmit;

VideoPlaybackRoughnessReporter::VideoPlaybackRoughnessReporter(
    ReportingCallback reporting_cb)
    : reporting_cb_(reporting_cb) {}

VideoPlaybackRoughnessReporter::~VideoPlaybackRoughnessReporter() = default;

double VideoPlaybackRoughnessReporter::ConsecutiveFramesWindow::roughness()
    const {
  return root_mean_square_error.InMillisecondsF();
}

VideoPlaybackRoughnessReporter::FrameInfo::FrameInfo() = default;
VideoPlaybackRoughnessReporter::FrameInfo::FrameInfo(const FrameInfo&) =
    default;

void VideoPlaybackRoughnessReporter::FrameSubmitted(
    TokenType token,
    const media::VideoFrame& frame,
    base::TimeDelta render_interval) {
  if (!frames_.empty() && viz::FrameTokenGT(frames_.back().token, token)) {
    DCHECK(false) << "Frames submitted out of order.";
    return;
  }

  FrameInfo info;
  info.token = token;
  info.decode_time = frame.metadata().decode_end_time;
  info.refresh_rate_hz = base::ClampRound(render_interval.ToHz());
  info.size = frame.natural_size();

  info.intended_duration = frame.metadata().wallclock_frame_duration;
  if (info.intended_duration) {
    if (render_interval > info.intended_duration.value()) {
      // In videos with FPS higher than display refresh rate we acknowledge
      // the fact that some frames will be dropped upstream and frame's intended
      // duration can't be less than refresh interval.
      info.intended_duration = render_interval;
    }

    // Adjust frame window size to fit about 1 second of playback
    const int win_size =
        base::ClampRound(info.intended_duration.value().ToHz());
    frames_window_size_ = std::clamp(win_size, kMinWindowSize, kMaxWindowSize);
  }

  frames_.push_back(info);
}

void VideoPlaybackRoughnessReporter::FramePresented(TokenType token,
                                                    base::TimeTicks timestamp,
                                                    bool reliable_timestamp) {
  for (auto& frame : base::Reversed(frames_)) {
    if (token == frame.token) {
      if (frame.decode_time.has_value()) {
        auto time_since_decode = timestamp - frame.decode_time.value();
        RecordUmaVideoFrameSubmitter(is_media_stream_, time_since_decode);
      }

      if (reliable_timestamp)
        frame.presentation_time = timestamp;
      break;
    }
    if (viz::FrameTokenGT(token, frame.token))
      break;
  }
}

void VideoPlaybackRoughnessReporter::SubmitPlaybackRoughness() {
  // 0-based index, how many times to step away from the begin().
  int index_to_submit = windows_seen_ * (100 - kPercentileToSubmit) / 100;
  if (index_to_submit < 0 ||
      index_to_submit >= static_cast<int>(worst_windows_.size())) {
    DCHECK(false);
    return;
  }

  auto it = worst_windows_.begin() + index_to_submit;

  Measurement measurement;
  measurement.frames = it->size;
  measurement.duration = it->intended_duration;
  measurement.roughness = it->roughness();
  measurement.freezing = max_single_frame_error_;
  measurement.refresh_rate_hz = it->refresh_rate_hz;
  measurement.frame_size = it->frame_size;
  reporting_cb_.Run(measurement);

  worst_windows_.clear();
  windows_seen_ = 0;
  max_single_frame_error_ = base::TimeDelta();
}

void VideoPlaybackRoughnessReporter::ReportWindow(
    const ConsecutiveFramesWindow& win) {
  worst_windows_.insert(win);
  if (worst_windows_.size() > max_worst_windows_size())
    worst_windows_.erase(std::prev(worst_windows_.end()));

  windows_seen_++;
  if (windows_seen_ >= kMaxWindowsBeforeSubmit)
    SubmitPlaybackRoughness();
}

void VideoPlaybackRoughnessReporter::ProcessFrameWindow() {
  if (static_cast<int>(frames_.size()) <= frames_window_size_) {
    // There is no window to speak of, let's wait and process it later.
    return;
  }

  // If possible populate duration for frames that don't have it yet.
  auto cur_frame_it = frames_.begin();
  auto next_frame_it = std::next(cur_frame_it);
  for (; next_frame_it != frames_.end(); cur_frame_it++, next_frame_it++) {
    FrameInfo& cur_frame = *cur_frame_it;
    const FrameInfo& next_frame = *next_frame_it;

    if (cur_frame.actual_duration.has_value())
      continue;

    if (!cur_frame.presentation_time.has_value() ||
        !next_frame.presentation_time.has_value()) {
      // We reached a frame that hasn't been presented yet, there is
      // no way to keep processing the window.
      break;
    }

    cur_frame.actual_duration = next_frame.presentation_time.value() -
                                cur_frame.presentation_time.value();
  }

  int items_to_discard = 0;
  const int max_buffer_size = 2 * frames_window_size_;
  // There is sufficient number of frames with populated |actual_duration|
  // let's calculate window metrics and report it.
  if (next_frame_it - frames_.begin() > frames_window_size_) {
    ConsecutiveFramesWindow win;
    bool observed_change_in_parameters = false;
    double mean_square_error_ms2 = 0.0;
    base::TimeDelta total_error;
    auto& first_frame = frames_.front();
    if (first_frame.presentation_time.has_value()) {
      win.first_frame_time = first_frame.presentation_time.value();
      win.refresh_rate_hz = first_frame.refresh_rate_hz;
      win.frame_size = first_frame.size;
    }

    for (auto i = 0; i < frames_window_size_; i++) {
      FrameInfo& frame = frames_[i];
      base::TimeDelta error;

      if (win.frame_size != frame.size ||
          win.refresh_rate_hz != frame.refresh_rate_hz) {
        observed_change_in_parameters = true;
        break;
      }

      if (frame.actual_duration.has_value() &&
          frame.intended_duration.has_value()) {
        error = frame.actual_duration.value() - frame.intended_duration.value();
        win.intended_duration += frame.intended_duration.value();
      }
      total_error += error;
      max_single_frame_error_ =
          std::max(max_single_frame_error_, error.magnitude());
      mean_square_error_ms2 +=
          total_error.InMillisecondsF() * total_error.InMillisecondsF();
    }
    win.size = frames_window_size_;
    win.root_mean_square_error = base::Milliseconds(
        std::sqrt(mean_square_error_ms2 / frames_window_size_));

    if (observed_change_in_parameters) {
      // There has been a change in the frame size or the screen refresh rate,
      // whatever roughness stats were accumulated up to this point need to be
      // reported or discarded, because there is no point in mixing together
      // roughess for different resolutions or refresh rates.
      if (windows_seen_ >= kMinWindowsBeforeSubmit) {
        SubmitPlaybackRoughness();
      } else {
        worst_windows_.clear();
        windows_seen_ = 0;
        max_single_frame_error_ = base::TimeDelta();
      }
    } else {
      ReportWindow(win);
    }

    // The frames in the window have been reported,
    // no need to keep them around any longer.
    items_to_discard = frames_window_size_;
  } else if (static_cast<int>(frames_.size()) > max_buffer_size) {
    // |frames_| grew too much, because apparently we're not getting consistent
    // FramePresented() calls and no smoothness windows can be reported.
    // Nevertheless, we can't allow |frames_| to grow too big, let's drop
    // the oldest items beyond |max_buffer_size|;
    items_to_discard = frames_.size() - max_buffer_size;
  }

  frames_.erase(frames_.begin(), frames_.begin() + items_to_discard);
}

void VideoPlaybackRoughnessReporter::Reset() {
  if (windows_seen_ >= kMinWindowsBeforeSubmit)
    SubmitPlaybackRoughness();
  frames_.clear();
  worst_windows_.clear();
  windows_seen_ = 0;
  max_single_frame_error_ = base::TimeDelta();
}

}  // namespace cc
