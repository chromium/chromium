// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_NOTIFICATION_ARC_VM_DATA_MIGRATION_NOTIFIER_H_
#define CHROME_BROWSER_ASH_ARC_NOTIFICATION_ARC_VM_DATA_MIGRATION_NOTIFIER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"
#include "chrome/browser/profiles/profile.h"

namespace arc {

// Manages a notification for ARCVM /data migration.
class ArcVmDataMigrationNotifier : public ArcSessionManagerObserver {
 public:
  explicit ArcVmDataMigrationNotifier(Profile* profile);

  ~ArcVmDataMigrationNotifier() override;

  ArcVmDataMigrationNotifier(const ArcVmDataMigrationNotifier&) = delete;
  ArcVmDataMigrationNotifier& operator=(const ArcVmDataMigrationNotifier&) =
      delete;

  // ArcSessinoManagerObserver overrides:
  void OnArcStarted() override;
  void OnArcSessionStopped(ArcStopReason reason) override;
  void OnArcSessionBlockedByArcVmDataMigration(
      bool auto_resume_enabled) override;

 private:
  const raw_ptr<Profile, DanglingUntriaged> profile_;

  void ShowNotification();

  void CloseNotification();

  void OnNotificationClicked(std::optional<int> button_index);

  void OnRestartAccepted(bool accepted);

  base::ScopedObservation<ArcSessionManager, ArcSessionManagerObserver>
      arc_session_observation_{this};

  base::WeakPtrFactory<ArcVmDataMigrationNotifier> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_NOTIFICATION_ARC_VM_DATA_MIGRATION_NOTIFIER_H_
