// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/presentation_time_recorder.h"

#include <memory>

#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/presentation_time_recorder.h"

namespace ash {

namespace {

// Wrapper class of ui::PresentationTimeRecorder that reports latency based
// on ui::Compositor.
class CompositorPresentationTimeRecorder : public PresentationTimeRecorder {
 public:
  CompositorPresentationTimeRecorder(aura::Window* window,
                                     const char* latency_histogram_name,
                                     const char* max_latency_histogram_name)
      : compositor_recorder_(ui::CreatePresentationTimeHistogramRecorder(
            window->layer()->GetCompositor(),
            latency_histogram_name,
            max_latency_histogram_name)) {}

  CompositorPresentationTimeRecorder(
      const CompositorPresentationTimeRecorder&) = delete;
  CompositorPresentationTimeRecorder& operator=(
      const CompositorPresentationTimeRecorder&) = delete;

  ~CompositorPresentationTimeRecorder() override = default;

  // PresentationTimeRecorder:
  bool RequestNext() override { return compositor_recorder_->RequestNext(); }

 private:
  std::unique_ptr<ui::PresentationTimeRecorder> compositor_recorder_;
};

}  // namespace

// static
std::unique_ptr<PresentationTimeRecorder>
PresentationTimeRecorder::CreateCompositorRecorder(
    aura::Window* window,
    const char* latency_histogram_name,
    std::optional<const char*> max_latency_histogram_name) {
  return std::make_unique<CompositorPresentationTimeRecorder>(
      window, latency_histogram_name,
      max_latency_histogram_name.has_value()
          ? max_latency_histogram_name.value()
          : "");
}

}  // namespace ash
