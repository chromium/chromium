// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_SESSION_ARC_SESSION_MANAGER_OBSERVER_H_
#define CHROME_BROWSER_ASH_ARC_SESSION_ARC_SESSION_MANAGER_OBSERVER_H_

#include "chrome/browser/ash/arc/arc_support_host.h"

namespace arc {

enum class ArcStopReason;

// Observer for those services outside of ARC which want to know ARC events.
class ArcSessionManagerObserver {
 public:
  // Called to notify that whether Google Play Store is enabled or not, which
  // is represented by "arc.enabled" preference, is updated.
  virtual void OnArcPlayStoreEnabledChanged(bool enabled) {}

  // Called with when user opted-in for ARC and ARC is going to be created for
  // this user. I.e. successful user action triggered ARC user instance
  // initialization.
  virtual void OnArcOptInUserAction() {}

  // Called to notify that checking of Android management status started
  // during the opt-in flow.
  virtual void OnArcOptInManagementCheckStarted() {}

  // Called to notify that ARC begins to start.
  virtual void OnArcStarted() {}

  // Called to notify that ARC has been successfully provisioned for the first
  // time after OptIn.
  virtual void OnArcInitialStart() {}

  // Called when ARC session is stopped, and is not being restarted
  // automatically.
  virtual void OnArcSessionStopped(ArcStopReason stop_reason) {}

  // Called when ARC session is stopped, but is being restarted automatically.
  // This is called _after_ the container is actually created.
  virtual void OnArcSessionRestarting() {}

  // Called to notify that Android data has been removed. Used in
  // browser_tests
  virtual void OnArcDataRemoved() {}

  // Called to notify that ARC start is delayed by ARC on demand
  virtual void OnArcStartDelayed() {}

  // Called to notify that the error is requested by the session manager to be
  // displayed in the support host. This is called even if Support UI is
  // disabled. Note that this is not called in cases when the support app
  // switches to an error page by itself.
  // |error| points to the top level error indicating the type of problem and an
  // optional argument to format the error message for display on the UI.
  virtual void OnArcErrorShowRequested(ArcSupportHost::ErrorInfo error_info) {}

  // Called with true when the /run/arc[vm]/host_generated/*.prop files are
  // generated (and false when the attempt fails.) The function is called once
  // per observer regardless of whether the attempt has already been made
  // before the observer is added.
  virtual void OnPropertyFilesExpanded(bool result) {}

  // Called when ARC session is blocked because ARCVM /data migration is in
  // progress. If |auto_resume_enabled| is true, the migration will be
  // automatically resumed by restarting the Chrome session. Otherwise the user
  // needs to manually enter the migration flow by clicking a notification.
  virtual void OnArcSessionBlockedByArcVmDataMigration(
      bool auto_resume_enabled) {}

  // Called when ARC session manager is shutting down.
  virtual void OnShutdown() {}

 protected:
  virtual ~ArcSessionManagerObserver() = default;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_SESSION_ARC_SESSION_MANAGER_OBSERVER_H_
