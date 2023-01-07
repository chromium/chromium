// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_PRESENTATION_TIME_RECORDER_H_
#define ASH_PUBLIC_CPP_PRESENTATION_TIME_RECORDER_H_

#include <memory>

#include "ash/public/cpp/ash_public_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace aura {
class Window;
}

namespace ash {

// A general interface for presentation time recording.
class ASH_PUBLIC_EXPORT PresentationTimeRecorder {
 public:
  // Creates a recorder tracking ui::Compositor.
  static std::unique_ptr<PresentationTimeRecorder> CreateCompositorRecorder(
      aura::Window* window,
      const char* latency_histogram_name,
      absl::optional<const char*> max_latency_histogram_name = absl::nullopt);

  virtual ~PresentationTimeRecorder() = default;

  // Prepare to record timin for UI changes. Invoked before making UI changes
  // so that the recorder could start to watch for UI changes if needed.
  virtual void PrepareToRecord() {}

  // Request to record timing for the next frame. Returns true if the request
  // is accepted. Otherwise, returns false.
  virtual bool RequestNext() = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_PRESENTATION_TIME_RECORDER_H_
