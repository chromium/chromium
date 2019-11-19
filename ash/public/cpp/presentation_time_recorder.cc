// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/presentation_time_recorder.h"

#include <ostream>

#include "base/callback.h"
#include "ui/gfx/presentation_feedback.h"

namespace ash {

namespace {

bool report_immediately_for_test = false;

std::string ToFlagString(uint32_t flags) {
  std::string tmp;
  if (flags & gfx::PresentationFeedback::kVSync)
    tmp += "V,";
  if (flags & gfx::PresentationFeedback::kFailure)
    tmp += "F,";
  if (flags & gfx::PresentationFeedback::kHWClock)
    tmp += "HCL,";
  if (flags & gfx::PresentationFeedback::kHWCompletion)
    tmp += "HCO,";
  if (flags & gfx::PresentationFeedback::kZeroCopy)
    tmp += "Z";
  return tmp;
}

}  // namespace

// PresentationTimeRecorderInternal -------------------------------------------

class PresentationTimeRecorder::PresentationTimeRecorderInternal
    : public ui::CompositorObserver {
 public:
  explicit PresentationTimeRecorderInternal(ui::Compositor* compositor)
      : compositor_(compositor) {
    compositor_->AddObserver(this);
    VLOG(1) << "Start Recording Frame Time";
  }
  ~PresentationTimeRecorderInternal() override {
    VLOG(1) << "Finished Recording FrameTime: average latency="
            << average_latency_ms() << "ms, max latency=" << max_latency_ms()
            << "ms, failure_ratio=" << failure_ratio();
    if (compositor_)
      compositor_->RemoveObserver(this);
  }

  // Start recording next frame. It skips requesting next frame and returns
  // false if the previous frame has not been committed yet.
  bool RequestNext();

  // ui::CompositorObserver:
  void OnCompositingDidCommit(ui::Compositor* compositor) override {
    // Skip updating the state if commit happened after present without
    // request because the commit is for unrelated activity.
    if (state_ != PRESENTED)
      state_ = COMMITTED;
  }
  void OnCompositingShuttingDown(ui::Compositor* compositor) override {
    DCHECK_EQ(compositor_, compositor);
    compositor_->RemoveObserver(this);
    compositor_ = nullptr;
  }

  // Mark the recorder to be deleted when the last presentation feedback
  // is reported.
  void EndRecording() {
    recording_ = false;
    if (state_ == PRESENTED)
      delete this;
  }

 protected:
  int max_latency_ms() const { return max_latency_ms_; }
  int success_count() const { return success_count_; }

 private:
  friend class TestApi;

  enum State {
    // The frame has been presented to the screen. This is the initial state.
    PRESENTED,
    // The presentation feedback has been requested.
    REQUESTED,
    // The changes to layers have been submitted, and waiting to be presented.
    COMMITTED,
  };

  // |delta| is the duration between the successful request time and
  // presentation time.
  virtual void ReportTime(base::TimeDelta delta) = 0;

  void OnPresented(int count,
                   base::TimeTicks requested_time,
                   const gfx::PresentationFeedback& feedback);

  int average_latency_ms() const {
    return success_count_ ? total_latency_ms_ / success_count_ : 0;
  }
  int failure_ratio() const {
    return failure_count_
               ? (100 * failure_count_) / (success_count_ + failure_count_)
               : 0;
  }

  State state_ = PRESENTED;

  int success_count_ = 0;
  int failure_count_ = 0;
  int request_count_ = 0;
  int total_latency_ms_ = 0;
  int max_latency_ms_ = 0;

  ui::Compositor* compositor_ = nullptr;
  bool recording_ = true;

  base::WeakPtrFactory<PresentationTimeRecorderInternal> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(PresentationTimeRecorderInternal);
};

bool PresentationTimeRecorder::PresentationTimeRecorderInternal::RequestNext() {
  if (!compositor_)
    return false;

  if (state_ == REQUESTED)
    return false;

  const base::TimeTicks now = base::TimeTicks::Now();

  VLOG(1) << "Start Next (" << request_count_
          << ") state=" << (state_ == COMMITTED ? "Committed" : "Presented");
  state_ = REQUESTED;

  if (report_immediately_for_test) {
    state_ = COMMITTED;
    gfx::PresentationFeedback feedback;
    OnPresented(request_count_++, now, feedback);
    return true;
  }

  compositor_->RequestPresentationTimeForNextFrame(
      base::BindOnce(&PresentationTimeRecorderInternal::OnPresented,
                     weak_ptr_factory_.GetWeakPtr(), request_count_++, now));
  return true;
}

void PresentationTimeRecorder::PresentationTimeRecorderInternal::OnPresented(
    int count,
    base::TimeTicks requested_time,
    const gfx::PresentationFeedback& feedback) {
  std::unique_ptr<PresentationTimeRecorderInternal> deleter;
  if (!recording_ && (count == (request_count_ - 1)))
    deleter = base::WrapUnique(this);

  if (state_ == COMMITTED)
    state_ = PRESENTED;

  if (feedback.flags & gfx::PresentationFeedback::kFailure) {
    failure_count_++;
    LOG(WARNING) << "PresentationFailed (" << count << "):"
                 << ", flags=" << ToFlagString(feedback.flags);
    return;
  }
  const base::TimeDelta delta = feedback.timestamp - requested_time;
  if (delta.InMilliseconds() > max_latency_ms_)
    max_latency_ms_ = delta.InMilliseconds();

  success_count_++;
  total_latency_ms_ += delta.InMilliseconds();
  ReportTime(delta);
  VLOG(1) << "OnPresented (" << count << "):" << delta.InMilliseconds()
          << ",flags=" << ToFlagString(feedback.flags);
}

// PresentationTimeRecorder ---------------------------------------------------

PresentationTimeRecorder::PresentationTimeRecorder(
    std::unique_ptr<PresentationTimeRecorderInternal> internal)
    : recorder_internal_(std::move(internal)) {}

PresentationTimeRecorder::~PresentationTimeRecorder() {
  auto* recorder_internal = recorder_internal_.release();
  // The internal recorder self destruct when finished its job.
  recorder_internal->EndRecording();
}

bool PresentationTimeRecorder::RequestNext() {
  return recorder_internal_->RequestNext();
}

// static
void PresentationTimeRecorder::SetReportPresentationTimeImmediatelyForTest(
    bool enable) {
  report_immediately_for_test = enable;
}

namespace {

base::HistogramBase* CreateTimesHistogram(const char* name) {
  return base::Histogram::FactoryTimeGet(
      name, base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromMilliseconds(200), 50,
      base::HistogramBase::kUmaTargetedHistogramFlag);
}

// PresentationTimeHistogramRecorder ------------------------------------------

class ASH_PUBLIC_EXPORT PresentationTimeHistogramRecorder
    : public PresentationTimeRecorder::PresentationTimeRecorderInternal {
 public:
  // |presentation_time_histogram_name| records latency reported on
  // |ReportTime()| and |max_latency_histogram_name| records the maximum latency
  // reported during the lifetime of this object.  Histogram names must be the
  // name of the UMA histogram defined in histograms.xml.
  PresentationTimeHistogramRecorder(
      ui::Compositor* compositor,
      const char* presentation_time_histogram_name,
      const char* max_latency_histogram_name)
      : PresentationTimeRecorderInternal(compositor),
        presentation_time_histogram_(
            CreateTimesHistogram(presentation_time_histogram_name)),
        max_latency_histogram_name_(max_latency_histogram_name) {}

  ~PresentationTimeHistogramRecorder() override {
    if (success_count() > 0) {
      CreateTimesHistogram(max_latency_histogram_name_.c_str())
          ->AddTimeMillisecondsGranularity(
              base::TimeDelta::FromMilliseconds(max_latency_ms()));
    }
  }

  // PresentationTimeRecorderInternal:
  void ReportTime(base::TimeDelta delta) override {
    presentation_time_histogram_->AddTimeMillisecondsGranularity(delta);
  }

 private:
  base::HistogramBase* presentation_time_histogram_;
  std::string max_latency_histogram_name_;

  DISALLOW_COPY_AND_ASSIGN(PresentationTimeHistogramRecorder);
};

}  // namespace

std::unique_ptr<PresentationTimeRecorder>
CreatePresentationTimeHistogramRecorder(
    ui::Compositor* compositor,
    const char* presentation_time_histogram_name,
    const char* max_latency_histogram_name) {
  return std::make_unique<PresentationTimeRecorder>(
      std::make_unique<PresentationTimeHistogramRecorder>(
          compositor, presentation_time_histogram_name,
          max_latency_histogram_name));
}

// TestApi --------------------------------------------------------------------

PresentationTimeRecorder::TestApi::TestApi(PresentationTimeRecorder* recorder)
    : recorder_(recorder) {}

void PresentationTimeRecorder::TestApi::OnCompositingDidCommit(
    ui::Compositor* compositor) {
  recorder_->recorder_internal_->OnCompositingDidCommit(compositor);
}

void PresentationTimeRecorder::TestApi::OnPresented(
    int count,
    base::TimeTicks requested_time,
    const gfx::PresentationFeedback& feedback) {
  recorder_->recorder_internal_->OnPresented(count, requested_time, feedback);
}

int PresentationTimeRecorder::TestApi::GetMaxLatencyMs() const {
  return recorder_->recorder_internal_->max_latency_ms();
}

int PresentationTimeRecorder::TestApi::GetSuccessCount() const {
  return recorder_->recorder_internal_->success_count();
}

int PresentationTimeRecorder::TestApi::GetFailureRatio() const {
  return recorder_->recorder_internal_->failure_ratio();
}

}  // namespace ash
