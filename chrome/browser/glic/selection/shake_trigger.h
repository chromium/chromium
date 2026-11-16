// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SELECTION_SHAKE_TRIGGER_H_
#define CHROME_BROWSER_GLIC_SELECTION_SHAKE_TRIGGER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/glic/selection/wiggle_detector_factory.h"

namespace blink {
class WebInputEvent;
class WebMouseEvent;
}  // namespace blink

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class PointF;
}  // namespace gfx

namespace glic {

class WiggleDetector;

class ShakeTriggerClient {
 public:
  virtual ~ShakeTriggerClient() = default;
  // Returns true if the text selection is shared for the current profile.
  virtual bool IsTextSelectionSharingEnabled() const = 0;
  // Returns true if the Glic side panel is open.
  virtual bool IsSidePanelOpen() const = 0;
};

// Watches pointer movement for a wiggle and starts region capture when one is
// detected. With the "should_count" param, also runs every available detector
// without acting on detections.
class ShakeTrigger {
 public:
  ShakeTrigger(content::WebContents* web_contents, ShakeTriggerClient& client);
  ShakeTrigger(const ShakeTrigger&) = delete;
  ShakeTrigger& operator=(const ShakeTrigger&) = delete;
  virtual ~ShakeTrigger();

  void OnInputEvent(const blink::WebInputEvent& event);

  // Returns true if mouse shake trigger is enabled by feature flag and pref.
  bool IsEnabled() const;

  // Returns true if OnInputEvent() needs mouse moves.
  bool ShouldObserveMouseMoves() const;

 protected:
  // Triggers Glic region capture when a mouse shake is detected.
  // Virtual for testing.
  virtual void TriggerRegionCapture();

 private:
  struct Detector {
    explicit Detector(std::unique_ptr<WiggleDetector> detector);
    Detector(Detector&&);
    Detector& operator=(Detector&&);
    ~Detector();

    std::unique_ptr<WiggleDetector> detector;
    // Samples before this time are ignored.
    base::TimeTicks cooldown_end;
  };

  void ProcessMouseMove(const blink::WebMouseEvent& mouse_event);
  // Returns true if `detector` detects a wiggle; it then cools down.
  bool AddSample(Detector& detector,
                 gfx::PointF position,
                 base::TimeTicks timestamp);
  void ResetDetectors();

  base::WeakPtr<content::WebContents> web_contents_;
  const raw_ref<ShakeTriggerClient> client_;

  const WiggleTiming timing_;
  Detector trigger_detector_;
  // Only with the "should_count" param. Detections are not acted on.
  std::vector<Detector> count_only_detectors_;

  base::TimeTicks last_move_time_;
  // Mouse moves or clicks before this time are ignored.
  base::TimeTicks ignore_until_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SELECTION_SHAKE_TRIGGER_H_
