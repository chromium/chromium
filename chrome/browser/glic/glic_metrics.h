// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_METRICS_H_
#define CHROME_BROWSER_GLIC_GLIC_METRICS_H_

#include <vector>

#include "base/callback_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/glic/glic.mojom.h"
#include "chrome/browser/glic/glic_enums.h"
#include "components/prefs/pref_change_registrar.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

class Profile;

namespace glic {

class GlicEnabling;
class GlicFocusedTabManager;
class GlicWindowController;

// Responsible for all glic web-client metrics, and all stateful glic metrics.
// Some stateless glic metrics are logged inline in the relevant code for
// convenience.
class GlicMetrics {
 public:
  GlicMetrics(Profile* profile, GlicEnabling* enabling);
  GlicMetrics(const GlicMetrics&) = delete;
  GlicMetrics& operator=(const GlicMetrics&) = delete;
  ~GlicMetrics();

  // See glic.mojom for details. These are events from the web client. The
  // lifetime of the web client is scoped to that of the window, so if these
  // methods are called then controller_ is guaranteed to exist.
  void OnUserInputSubmitted(mojom::WebClientMode mode);
  void OnResponseStarted();
  void OnResponseStopped();
  void OnSessionTerminated();
  void OnResponseRated(bool positive);

  // ----Public API called by other glic classes-----
  // Called when the glic window starts to open.
  void OnGlicWindowOpen(bool attached, InvocationSource source);
  // Called when the glic window finishes closing.
  void OnGlicWindowClose();
  // Called when the glic window attaches or detaches.

  // Must be called immediately after constructor before any calls from
  // glic.mojom.
  void SetControllers(GlicWindowController* window_controller,
                      GlicFocusedTabManager* tab_manager);

  // Must be called when context is requested.
  void DidRequestContextFromFocusedTab();

 private:
  // Called when `impression_timer_` fires.
  void OnImpressionTimerFired();

  // Stores the source id at the time that context is requested.
  void StoreSourceId();

  // Called when enabled changes.
  void OnEnabledChanged();

  // Called when kGlicPinnedToTabstrip changes.
  void OnPinningPrefChanged();

  // These members are cleared in OnResponseStopped.
  base::TimeTicks input_submitted_time_;
  mojom::WebClientMode input_mode_;
  bool did_request_context_ = false;

  // Session state. `session_start_time_` is a sentinel that is cleared in
  // OnGlicWindowClose() and is used to determine whether OnGlicWindowOpen was
  // called.
  int session_responses_ = 0;
  base::TimeTicks session_start_time_;
  InvocationSource invocation_source_ = InvocationSource::kOsButton;

  // Used to record impressions of glic entry points.
  base::RepeatingTimer impression_timer_;

  // A context-free source id used when no web contents is targeted.
  ukm::SourceId no_url_source_id_;
  // The source id at the time context is requested. If context was not
  // requested then this is `no_url_source_id_`.
  ukm::SourceId source_id_;

  // The owner of this class is responsible for maintaining appropriate lifetime
  // for controller_.
  raw_ptr<GlicWindowController> window_controller_;
  raw_ptr<GlicFocusedTabManager> tab_manager_;
  raw_ptr<Profile> profile_;
  raw_ptr<GlicEnabling> enabling_;

  // Cache the last value so that we only emit metrics for changes to the last
  // value.
  bool is_enabled_ = false;

  // Set to true in OnResponseStarted() and set to false in OnResponseStopped().
  // This is a workaround and should be removed, see crbug.com/399151164.
  bool response_started_ = false;

  // Holds subscriptions for callbacks.
  std::vector<base::CallbackListSubscription> subscriptions_;

  // Cache the last value of the kGlicPinnedToTabstrip pref so that we only emit
  // metrics for changes to the last value.
  bool is_pinned_ = false;
  PrefChangeRegistrar pref_registrar_;
};

}  // namespace glic
#endif  // CHROME_BROWSER_GLIC_GLIC_METRICS_H_
