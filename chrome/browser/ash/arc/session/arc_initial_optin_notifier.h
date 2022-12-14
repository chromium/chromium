// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_SESSION_ARC_INITIAL_OPTIN_NOTIFIER_H_
#define CHROME_BROWSER_ASH_ARC_SESSION_ARC_INITIAL_OPTIN_NOTIFIER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace arc {

// Observes Arc session manager opt-in status, and notifies metrics.
class ArcInitialOptInNotifier : public ArcSessionManagerObserver,
                                public KeyedService {
 public:
  // Returns singleton instance for the given Profile.
  static ArcInitialOptInNotifier* GetForProfile(Profile* profile);

  explicit ArcInitialOptInNotifier(content::BrowserContext* context);

  ArcInitialOptInNotifier(const ArcInitialOptInNotifier&) = delete;
  ArcInitialOptInNotifier& operator=(const ArcInitialOptInNotifier&) = delete;

  ~ArcInitialOptInNotifier() override;

  // ArcSessionManagerObserver:
  void OnArcInitialStart() override;
  void OnArcOptInUserAction() override;

 private:
  // Must be the last member.
  base::WeakPtrFactory<ArcInitialOptInNotifier> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_SESSION_ARC_INITIAL_OPTIN_NOTIFIER_H_
