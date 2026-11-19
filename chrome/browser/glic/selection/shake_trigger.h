// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SELECTION_SHAKE_TRIGGER_H_
#define CHROME_BROWSER_GLIC_SELECTION_SHAKE_TRIGGER_H_

#include <list>
#include <memory>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/glic/public/glic_provider_metrics.h"
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
// to record wiggle detection and follow-up metrics.
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

  // Returns true if wiggles are counted (the "should_count" param).
  bool IsCounting() const;

  // Called when the user opens glic or selects text. Used to measure how
  // often a count-only detection is followed by either.
  void OnFollowUpAction();

 protected:
  // Triggers Glic region capture when a mouse shake is detected.
  // Virtual for testing.
  virtual void TriggerRegionCapture();

 private:
  struct Detector {
    explicit Detector(WiggleDetectorType type);
    Detector(Detector&&) noexcept;
    Detector& operator=(Detector&&) noexcept;
    ~Detector();

    WiggleDetectorType type;
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
  void RecordDetection(WiggleDetectorType type);
  void RecordFollowedUp(WiggleDetectorType type, bool followed);
  // Records the oldest detection as not followed up.
  void OnFollowUpWindowExpired();

  base::WeakPtr<content::WebContents> web_contents_;
  const raw_ref<ShakeTriggerClient> client_;
  const GlicMetricReporter metric_reporter_;

  const WiggleTiming timing_;
  Detector trigger_detector_;
  // Only with the "should_count" param. Detections are not acted on.
  std::vector<Detector> count_only_detectors_;

  base::TimeTicks last_move_time_;
  // Mouse moves or clicks before this time are ignored.
  base::TimeTicks ignore_until_;

  // For each detection, there is a 10s window where the user can trigger
  // glic. This list holds one timer for each of those 10s windows.
  //
  // time (s)        0   2   4   6   8   10  12  14  16
  //                 |---|---|---|---|---|---|---|---|
  // LS   start 0:00 [===================x              -> false (0:10)
  // L0   start 0:01   [===================x            -> false (0:11)
  // LS   start 0:03       [===================x        -> false (0:13)
  // L1   start 0:06             [===============*      -> true  (0:14)
  // timer fires                         ^ ^   ^
  // OnFollowUpAction()                          ^
  //
  // At T=10s, the 0:00 LS timer fires and records false (no glic use).
  // At T=11s, the 0:01 L0 timer fires and records false.
  // At T=13s, the 0:03 LS timer fires and records false.
  // Lastly the user uses glic at T=14s. L1 records true.
  //
  // Oldest first. Every window is the same length, so the oldest expires first.
  struct PendingFollowUp {
    explicit PendingFollowUp(WiggleDetectorType type);
    ~PendingFollowUp();

    const WiggleDetectorType type;
    base::OneShotTimer timer;
  };
  std::list<PendingFollowUp> pending_follow_ups_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SELECTION_SHAKE_TRIGGER_H_
