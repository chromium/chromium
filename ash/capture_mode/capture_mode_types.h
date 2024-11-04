// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_TYPES_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_TYPES_H_

namespace ash {

// Specifies the camera preview snap position, it can be one of the four corners
// of the surface being recorded. Note that these values are persisted to
// histograms so existing values should remain unchanged and new values should
// be added to the end.
enum class CameraPreviewSnapPosition {
  kTopLeft = 0,
  kBottomLeft,
  kBottomRight,
  kTopRight,
  kMaxValue = kTopRight,
};

// Defines the capture type Capture Mode is currently using.
enum class CaptureModeType {
  kImage,
  kVideo,
};

// Defines the source of the capture used by Capture Mode.
enum class CaptureModeSource {
  kFullscreen,
  kRegion,
  kWindow,
};

// Specifies the capture mode allowance types.
enum class CaptureAllowance {
  // Capture mode is allowed.
  kAllowed,
  // Capture mode is blocked due to admin-enforced device policy.
  kDisallowedByPolicy,
  // Video recording is blocked due to app- or content- enforced content
  // protection. Applies only to video recording.
  kDisallowedByHdcp
};

// The position of the press event during the fine tune phase of a region
// capture session. This will determine what subsequent drag events do to the
// select region.
enum class FineTunePosition {
  // The initial press was outside region. Subsequent drags will do nothing.
  kNone,
  // The initial press was inside the select region. Subsequent drags will
  // move the entire region.
  kCenter,
  // The initial press was on one of the drag affordance circles in the corners.
  // Subsequent drags will resize the region. These are sorted clockwise
  // starting at the top left.
  kTopLeftVertex,
  kTopRightVertex,
  kBottomRightVertex,
  kBottomLeftVertex,
  // The initial press was along the edges of the region. Subsequent drags will
  // resize the region, but only along one dimension. These are sorted clockwise
  // starting at the top.
  kTopEdge,
  kRightEdge,
  kBottomEdge,
  kLeftEdge,
};

// Defines the supported recording formats.
enum class RecordingType {
  kWebM,
  kGif,
};

// Defines the supported audio recording modes. Note that these values are
// persisted to histograms so existing values should remain unchanged and new
// values should be added to the end.
enum class AudioRecordingMode {
  kOff = 0,
  kSystem,
  kMicrophone,
  kSystemAndMicrophone,
  kMaxValue = kSystemAndMicrophone,
};

// Specifies the capture mode behavior types.
enum class BehaviorType {
  kDefault,
  kProjector,
  kGameDashboard,
  kSunfish,
};

// Converts the enum class `RecordingType` to its integer value.
constexpr int ToInt(RecordingType type) {
  return static_cast<int>(type);
}

// The concrete type of the capture session instance.
enum class SessionType {
  kNull,
  kReal,
};

// Defines the type of action button used for a selected region. Higher enum
// values correspond to higher ranks. Buttons of same type are grouped together.
enum class ActionButtonType {
  kOther,
  kScanner,
  kCopyText,
  kSunfish,
};

// Defines the capture type to be performed and how the captured image will be
// used.
enum class PerformCaptureType {
  // Captured from normal capture mode to take a screenshot.
  kCapture,
  // Captured when the "search" button is pressed from normal capture mode. The
  // captured image will be sent to a search service.
  kSearch,
  // Captured when the "smart actions" button is pressed from normal capture
  // mode. The captured image will be sent to the Scanner service.
  kScanner,
  // Captured when a region is selected from normal capture mode. The captured
  // image will be processed by on-device OCR to detect text.
  kTextDetection,
  // Captured when a region is selected from Sunfish mode. The captured image
  // will be sent to a search service and the Scanner service.
  kSunfish,
};

// Defines the rank of an action button for a selected region. Higher ranked
// buttons are placed further to the right than lower ranked ones. Lower ranked
// buttons may not be visible if there are too many action buttons available.
struct ActionButtonRank {
  ActionButtonRank(ActionButtonType type, int weight)
      : type(type), weight(weight) {}

  // Returns `true` if `this` is less than `rhs`. The comparison is done
  // by `type` first, then by `weight`.
  bool operator<(const ActionButtonRank& rhs) const {
    if (type == rhs.type) {
      return weight < rhs.weight;
    }

    return type < rhs.type;
  }

  ActionButtonType type;

  // Buttons are weighted against other buttons of the same `ActionButtonType`.
  // For example, an `ActionButtonType::kDefault` button with weight 1 will have
  // a higher rank compared to an `ActionButtonType::kDefault` button with
  // weight 0, and thus be placed to its right.
  int weight;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_TYPES_H_
