// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_DECISION_DETAILS_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_DECISION_DETAILS_H_

#include <string>
#include <vector>

namespace ukm {
namespace builders {
class TabManager_LifecycleStateChange;
}
}  // namespace ukm

namespace resource_coordinator {

// An enumeration of reasons why a particular intervention or lifecycle state
// changes can be denied. This is a superset of all failure reasons that can
// apply for any particular intervention. New reasons can freely be added to
// this enum as necessary, but UKM plumbing and string conversion needs to be
// maintained as well.
enum class DecisionFailureReason : int32_t {
  // An invalid failure reason. This must remain first.
  INVALID = -1,
  // The browser was opted out of the intervention via enterprise policy.
  LIFECYCLES_ENTERPRISE_POLICY_OPT_OUT,
  // A frame on the page opted itself out of the intervention via origin trial.
  ORIGIN_TRIAL_OPT_OUT,
  // The origin was opted out of the intervention in the global disallowlist.
  GLOBAL_DISALLOWLIST,
  // The local heuristic opted the origin out of the intervention due to its use
  // of audio while in the background.
  HEURISTIC_AUDIO,
  // The local heuristic opted the origin out of the intervention due to its use
  // of favicon updates while in the background.
  HEURISTIC_FAVICON,
  // The local heuristic is temporarily opting the origin out of the
  // intervention due to a lack of sufficient observation time.
  HEURISTIC_INSUFFICIENT_OBSERVATION,
  // The local heuristic opted the origin out of the intervention due to its use
  // of title updates while in the background.
  HEURISTIC_TITLE,
  // The tab is opted out of the intervention as it is currently capturing user
  // media (webcam, microphone, etc).
  LIVE_STATE_CAPTURING,
  // The tab is opted out of the intervention by an extension.
  LIVE_STATE_EXTENSION_DISALLOWED,
  // The tab is opted out of the intervention as it contains text form entry.
  LIVE_STATE_FORM_ENTRY,
  // The tab is opted out of the intervention as it is currently hosting a PDF.
  LIVE_STATE_IS_PDF,
  // The tab is opted out of the intervention as it is currently being mirrored
  // (casting, etc).
  LIVE_STATE_MIRRORING,
  // The tab is opted out of the intervention as it is currently playing audio.
  LIVE_STATE_PLAYING_AUDIO,
  // The tab is opted out of the intervention as it is currently using
  // WebSockets.
  // NOTE: This heuristic isn't used in the freezing/discarding interventions.
  LIVE_STATE_USING_WEB_SOCKETS,
  // The tab is opted out of the intervention as it is currently WebUSB.
  LIVE_STATE_USING_WEB_USB,
  // The tab is opted out of the intervention as it is currently visible.
  LIVE_STATE_VISIBLE,
  // The tab is opted out of the intervention as it's currently using DevTools.
  LIVE_STATE_DEVTOOLS_OPEN,
  // The tab is opted out of the intervention as it's currently capturing a
  // window or screen.
  LIVE_STATE_DESKTOP_CAPTURE,
  // This tab is sharing its BrowsingInstance with another tab, and so could
  // want to communicate with it.
  LIVE_STATE_SHARING_BROWSING_INSTANCE,
  // The tab is opted out of the intervention as it's currently connected to a
  // bluetooth device.
  LIVE_STATE_USING_BLUETOOTH,
  // The tab is opted out of the intervention as it's currently holding at least
  // one WebLock.
  LIVE_STATE_USING_WEBLOCK,
  // The tab is opted out of the intervention as it's currently holding at least
  // one IndexedDB lock.
  LIVE_STATE_USING_INDEXEDDB_LOCK,
  // The tab is opted out of the intervention as it has the permission to use
  // notifications.
  LIVE_STATE_HAS_NOTIFICATIONS_PERMISSION,
  // The tab is a standalone desktop PWA window.
  LIVE_WEB_APP,
  // The tab is displaying content in picture-in-picture.
  LIVE_PICTURE_IN_PICTURE,
  // This must remain last.
  MAX,
};

// An enumeration of reasons why a particular intervention or lifecycle state
// change can be approved. The fact that no "live state" failures are blocking
// the intervention is implicit, and doesn't need to be explicitly encoded.
enum class DecisionSuccessReason : int32_t {
  // An invalid failure reason. This must remain first.
  INVALID = -1,
  // A frame on the page opted itself in the intervention via origin trial.
  ORIGIN_TRIAL_OPT_IN,
  // The origin was opted into the intervention via the global allowlist.
  GLOBAL_ALLOWLIST,
  // The origin has been observed to be safe for the intervention using local
  // database observations.
  HEURISTIC_OBSERVED_TO_BE_SAFE,
  // This must remain last.
  MAX,
};

// Helper function for converting a reason to a string representation.
const char* ToString(DecisionFailureReason failure_reason);
const char* ToString(DecisionSuccessReason success_reason);

// Describes the detailed reasons why a particular intervention decision was
// made. This is populated by the various policy bits of policy logic that
// decide whether a particular intervention or lifecycle state transition can be
// performed. It can populate various related UKM builders and also be converted
// to a collection of user readable strings for the purposes of displaying in
// in web UI.
//
// A decision can contain multiple reasons for success or failure, and policy
// allows some success reasons to override some failure reasons and vice versa.
// The first reason posted to this object determines whether or not the overall
// outcome is positive or negative. It is assumed that reasons are posted in
// order of decreasing priority.
//
// For logging and inspection it is useful to know of all possible failure
// reasons blocking a success. Similarly, it is interesting to know of all of
// the possible success reasons blocking a failure. To this end, policy logic
// should continue populating the decision with details until is has "toggled".
// That is, a success reason has followed a chain of failures or vice versa.
// The "toggling" of the decision chain is indicated by the return value from
// AddReason. This allows writing code like the following:
//
// bool DecideIfCanDoSomething(DecisionDetails* details) {
//   if (some_condition_is_false) {
//     if (details->AddReason(kSomeFailureReason))
//       return details->IsPositive();
//   }
//   if (some_other_condition) {
//     if (details->AddReason(kSomeOtherFailureReason))
//       return details->IsPositive();
//   }
//   ...
// }
class DecisionDetails {
 public:
  // A union of success/failure reasons. This is allowed to be copied in order
  // to be compatible with STL containers.
  class Reason {
   public:
    Reason();
    explicit Reason(DecisionSuccessReason success_reason);
    explicit Reason(DecisionFailureReason failure_reason);
    Reason(const Reason& rhs);
    ~Reason();

    Reason& operator=(const Reason& rhs);

    bool IsValid() const;
    bool IsSuccess() const;
    bool IsFailure() const;
    DecisionSuccessReason success_reason() const;
    DecisionFailureReason failure_reason() const;

    const char* ToString() const;

    bool operator==(const Reason& rhs) const;
    bool operator!=(const Reason& rhs) const;

   private:
    DecisionSuccessReason success_reason_;
    DecisionFailureReason failure_reason_;
  };

  DecisionDetails();

  DecisionDetails(const DecisionDetails&) = delete;
  DecisionDetails& operator=(const DecisionDetails&) = delete;

  ~DecisionDetails();

  // Allow move assignment.
  DecisionDetails& operator=(DecisionDetails&& rhs);

  // Adds a success or failure reason. Returns true if the chain of reasons has
  // "toggled", false otherwise.
  bool AddReason(const Reason& reason);
  bool AddReason(DecisionFailureReason failure_reason);
  bool AddReason(DecisionSuccessReason success_reason);

  // Returns the outcome of the decision. This is implicit from the reasons that
  // have been posted to this object.
  bool IsPositive() const;

  // Returns the main success reason. This is only valid to call if IsPositive
  // is true.
  DecisionSuccessReason SuccessReason() const;

  // Returns the main failure reason. This is only valid to call if IsPositive
  // is false.
  DecisionFailureReason FailureReason() const;

  // Returns the full vector of reasons.
  const std::vector<Reason>& reasons() const { return reasons_; }

  // Returns whether or not the chain of reasons has toggled.
  bool toggled() const { return toggled_; }

  // Populates the provided "TabManager.LifecycleStateChange" UKM builder with
  // information from this object.
  void Populate(ukm::builders::TabManager_LifecycleStateChange* ukm) const;

  // Returns a collection of failure reason strings, from most important failure
  // reason to least important. This is empty if the outcome is positive, and
  // will only be populated with failure reasons that are not overridden by any
  // success reasons.
  std::vector<std::string> GetFailureReasonStrings() const;

  void Clear();

 private:
  bool CheckIfToggled();

  // This is true if the vector of success reasons has "toggled" from all
  // failures to some successes, or vice versa. Continuing to collect additional
  // reasons after this toggle isn't very informative.
  bool toggled_;
  std::vector<Reason> reasons_;
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_DECISION_DETAILS_H_
