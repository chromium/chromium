// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_METRICS_GLIC_METRICS_SESSION_MANAGER_H_
#define CHROME_BROWSER_GLIC_SERVICE_METRICS_GLIC_METRICS_SESSION_MANAGER_H_

#include <memory>
#include <optional>
#include <set>

#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/glic/host/glic.mojom.h"

namespace glic {

class ActiveSession;
class GlicInstanceMetrics;
enum class GlicInstanceEvent;

// LINT.IfChange(GlicMultiInstanceSessionEndReason)
enum class GlicMultiInstanceSessionEndReason {
  kInactivity,
  kHidden,
  kOwnerDestroyed,
  kMaxValue = kOwnerDestroyed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicMultiInstanceSessionEndReason)

// Manages the lifecycle of a Glic metrics session. A session is a period of
// continuous user engagement with a Glic instance.
//
// === Session Lifecycle ===
//
// Session Start:
// A session begins when either of the following occurs:
// 1. The instance has been continuously visible for a configured duration (see
//    `kGlicMetricsSessionStartTimeout`).
// 2. The user submits input (text or audio) to the instance.
//
// Session End:
// A session ends under one of the following conditions:
// 1. Inactivity: The instance is visible but remains inactive for a configured
//    duration (see `kGlicMetricsSessionInactivityTimeout`). An instance is
//    active if its embedder is showing in an active browser window. A brief
//    re-activation (see `kGlicMetricsSessionRestartDebounceTimer`) will be
//    ignored and will not reset the inactivity timer.
// 2. Hidden: The instance is hidden for a configured duration (see
//    `kGlicMetricsSessionHiddenTimeout`). A brief period of visibility (see
//    `kGlicMetricsSessionRestartDebounceTimer`) will be ignored and will not
//    reset the hidden timer. Note that this does not currently handle occlusion
//    so even if an instance is open in a side panel and covered by another app
//    it is considered visible.
// 3. Destruction: The owning GlicInstanceMetrics object is destroyed.
//
// The session is represented by the lifetime of the internal `ActiveSession`
// object.
class GlicMetricsSessionManager {
 public:
  explicit GlicMetricsSessionManager(GlicInstanceMetrics* owner);
  ~GlicMetricsSessionManager();

  GlicMetricsSessionManager(const GlicMetricsSessionManager&) = delete;
  GlicMetricsSessionManager& operator=(const GlicMetricsSessionManager&) =
      delete;

  void OnVisibilityChanged(bool is_visible);
  void OnActivationChanged(bool is_active);
  void OnUserInputSubmitted(mojom::WebClientMode mode);
  void OnOwnerDestroyed();
  void OnEvent(GlicInstanceEvent event);
  void SetPinnedTabCount(int tab_count);
  int GetEventCount(GlicInstanceEvent event);

 private:
  friend class ActiveSession;

  void NotifySessionStarted();
  void FinishSession(GlicMultiInstanceSessionEndReason reason);
  void CreatePendingSession();

  const raw_ptr<GlicInstanceMetrics> owner_;

  // Holds the state and timers for a single, active session.
  // Its lifetime defines the duration of the session.
  std::unique_ptr<ActiveSession> active_session_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_METRICS_GLIC_METRICS_SESSION_MANAGER_H_
