// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_METRICS_H_
#define CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_METRICS_H_

#include <optional>

#include "chrome/browser/glic/public/glic_provider_metrics.h"
#include "components/sharing_message/proto/glic_experimental_triggering.pb.h"

class Profile;

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
  kScriptToolsDisabled = 16,
  kNonScriptToolAction = 17,
  // No open tab matched the document identifier of a requested action, e.g.
  // because the user navigated away or closed the tab.
  kTabNotFound = 18,
  // A request arrived for a context that is already executing a task.
  kTaskAlreadyRunning = 19,
  // The profile is not allowed to act on the web, either because of enterprise
  // policy or because the account is not eligible.
  kCannotActOnWeb = 20,
  kDeferredForBackgroundOptIn = 21,
  kOptInDialogAlreadyShowing = 22,
  kMaxValue = kOptInDialogAlreadyShowing,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicExperimentalTriggeringIncomingMessageResult)

// The execution outcome or failure reason for an experimental triggering
// actuation request.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(GlicExperimentalTriggeringExecutionOutcome)
enum class GlicExperimentalTriggeringExecutionOutcome {
  kSuccess = 0,
  kTimeoutWaitingForClient = 1,
  kTimeoutWaitingForActuation = 2,
  kInvokeErrorInvokeInProgress = 3,
  kInvokeErrorTabClosed = 4,
  kInvokeErrorOther = 5,
  kUpdatesRegistrationFailed = 6,
  kClientDisconnectedBeforeResponse = 7,
  kTerminalFailed = 8,
  kTerminalStopped = 9,
  kDestroyedBeforeCompletion = 10,
  kMaxValue = kDestroyedBeforeCompletion,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicExperimentalTriggeringExecutionOutcome)

// The reason an ExperimentalTriggeringUpdatesHandler was destroyed.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(GlicExperimentalTriggeringHandlerDestroyReason)
enum class GlicExperimentalTriggeringHandlerDestroyReason {
  // Handler finished with no actuation session and no live work (e.g.,
  // completed opt-in or script-tool action, or a one-shot request on a context
  // with no active session).
  kIdle = 0,
  // Actuation session ended with COMPLETE.
  kSessionComplete = 1,
  // Actuation session ended with STOPPED (StopActuationRequest or
  // kTerminalStopped).
  kSessionStopped = 2,
  // Actuation session ended with FAILED reported by the web client
  // (kTerminalFailed or SubscriberObservationType::kError).
  kSessionFailed = 3,
  // Session ended because Chrome synchronously replied FAILED to an incoming
  // request (e.g., refused initial trigger, missing metadata or payload,
  // version mismatch, or failed continuation precondition).
  kRejectedSynchronously = 4,
  // Glic invocation failed with a GlicInvokeError other than supersession, tab
  // closure, or instance destruction (e.g., timeout, invoke in progress, live
  // mode active, or client load error).
  kInvokeError = 5,
  // Session was superseded by another non-continuation Glic invocation
  // (GlicInvokeError::kSuperseded).
  kSuperseded = 6,
  // Target tab closed before completion (GlicInvokeError::kTabClosed,
  // FailPreparedTabClosed(), or InstanceTeardownReason::kTabClosed).
  kTabClosed = 7,
  // GlicInstance was destroyed before completion
  // (GlicInvokeError::kInstanceDestroyed* or
  // InstanceTeardownReason::kInstanceDestroyed).
  kInstanceDestroyed = 8,
  // Web client Mojo pipe disconnected unexpectedly (including reconnect grace
  // timeout).
  kClientDisconnected = 9,
  // Failed to register the updates handler with
  // GlicExperimentalTriggeringManager (or manager unavailable).
  kRegistrationFailed = 10,
  // Coordinator was destroyed while the handler had not ended its session.
  kCoordinatorShutdown = 11,
  // Invariant-violation safety-net bucket: recorded if a handler is destroyed
  // without going through OnUpdatesHandlerCleanup() or coordinator shutdown, or
  // if OnUpdatesHandlerCleanup() destroys a handler that still has live work or
  // an unfinished actuation session.
  kUnexpectedLiveDestruction = 12,
  kMaxValue = kUnexpectedLiveDestruction,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicExperimentalTriggeringHandlerDestroyReason)

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

  explicit ScopedIncomingMessageResultLogger(GlicMetricReporter reporter,
                                             Channel channel);

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

  GlicMetricReporter metric_reporter_;
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
    const GlicMetricReporter& reporter,
    const components_sharing_message::GlicExperimentalTriggering& triggering);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_METRICS_H_
