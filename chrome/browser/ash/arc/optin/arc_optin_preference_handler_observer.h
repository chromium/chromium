// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_OPTIN_ARC_OPTIN_PREFERENCE_HANDLER_OBSERVER_H_
#define CHROME_BROWSER_ASH_ARC_OPTIN_ARC_OPTIN_PREFERENCE_HANDLER_OBSERVER_H_

namespace arc {

// Notifies about changes in ARC related preferences and metrics mode.
class ArcOptInPreferenceHandlerObserver {
 public:
  // Notifies that the metrics mode has been changed.
  virtual void OnMetricsModeChanged(bool enabled, bool managed) = 0;
  // Notifies that the backup and restore mode has been changed.
  virtual void OnBackupAndRestoreModeChanged(bool enabled, bool managed) = 0;
  // Notifies that the location service mode has been changed.
  virtual void OnLocationServicesModeChanged(bool enabled, bool managed) = 0;

  virtual ~ArcOptInPreferenceHandlerObserver() = default;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_OPTIN_ARC_OPTIN_PREFERENCE_HANDLER_OBSERVER_H_
