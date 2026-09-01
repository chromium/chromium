// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_METRICS_H_
#define CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_METRICS_H_

#include <optional>

#include "components/sharing_message/proto/glic_experimental_triggering.pb.h"

namespace glic {

// The processing outcome or early return / failure reason for an incoming
// experimental triggering message.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(GlicExperimentalTriggeringIncomingMessageResult)
enum class GlicExperimentalTriggeringIncomingMessageResult {
  kSuccess = 0,
  kMissingTaskMetadata = 1,
  kVersionMismatchOrUnavailable = 2,
  kMissingServerChannel = 3,
  kMissingPayload = 4,
  kCoordinatorUnavailable = 5,
  kUserNotOptedIn = 6,
  kNoBrowserWindow = 7,
  kGlicServiceUnavailable = 8,
  kNoInstance = 9,
  kTriggeringManagerUnavailable = 10,
  kNoWebContentsForOptIn = 11,
  kAndroidOptInUnsupported = 12,
  kNoActionableRequest = 13,
  kUnexpectedRequestPayload = 14,
  // A processing path exited without setting a result. Any samples in this
  // bucket indicate an instrumentation gap that should be fixed.
  kResultNotSet = 15,
  kMaxValue = kResultNotSet,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicExperimentalTriggeringIncomingMessageResult)

// Helper RAII class to log the
// Glic.ExperimentalTriggering.IncomingMessageResult.{Channel} histogram for
// the channel passed to the constructor. Enforces that a result is set on
// destruction (via DCHECK). Supports move semantics so logging responsibility
// can be passed along the processing chain (e.g. from the transit-layer
// message handler into the coordinator and its handler methods).
class ScopedIncomingMessageResultLogger {
 public:
  // The channel over which the incoming message was received. Selects the
  // histogram suffix, so each channel gets its own
  // Glic.ExperimentalTriggering.IncomingMessageResult.{Channel} histogram.
  // Keep in sync with the Channel token variants in
  // //tools/metrics/histograms/metadata/glic/histograms.xml.
  enum class Channel {
    kSharingMessage,
    kBrowserActuatorTransport,
  };

  explicit ScopedIncomingMessageResultLogger(Channel channel);

  // Move operations transfer logging responsibility to the new instance and
  // mark the moved-from instance as disarmed so its destructor will do nothing
  // and will not trigger the DCHECK.
  ScopedIncomingMessageResultLogger(
      ScopedIncomingMessageResultLogger&& other) noexcept;
  ScopedIncomingMessageResultLogger& operator=(
      ScopedIncomingMessageResultLogger&& other) noexcept;

  ScopedIncomingMessageResultLogger(const ScopedIncomingMessageResultLogger&) =
      delete;
  ScopedIncomingMessageResultLogger& operator=(
      const ScopedIncomingMessageResultLogger&) = delete;

  ~ScopedIncomingMessageResultLogger();

  void set_result(GlicExperimentalTriggeringIncomingMessageResult result);

 private:
  // Logs the result (kResultNotSet if none was set) and disarms this instance.
  // No-op if already disarmed (moved from).
  void LogAndDisarm();

  Channel channel_;
  std::optional<GlicExperimentalTriggeringIncomingMessageResult> result_;

  // Tracks whether this instance has been moved from. When moved, disarmed_ is
  // set to true so the moved-from instance does not log or trigger a DCHECK
  // on destruction.
  bool disarmed_ = false;
};

// Records the approximate time elapsed between when the server sent the initial
// experimental triggering sharing message and when Chrome received it. This is
// only logged for the first sharing message arriving in Chrome (i.e.
// DeviceOptInRequest or TriggerActuationRequest). If the calculated duration is
// negative due to clock skew between client and server, it is clamped to 0.
void MaybeRecordInitialSharingMessageDeliveryLatency(
    const components_sharing_message::GlicExperimentalTriggering& triggering);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_METRICS_H_
