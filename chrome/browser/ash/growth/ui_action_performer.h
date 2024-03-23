// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GROWTH_UI_ACTION_PERFORMER_H_
#define CHROME_BROWSER_ASH_GROWTH_UI_ACTION_PERFORMER_H_

#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/components/growth/action_performer.h"

// Implements a base action performer to show visial elements, such a nudge and
// a notification.
class UiActionPerformer : public growth::ActionPerformer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Trigger when calling to show the UI. The UI may or may not show.
    virtual void OnReadyToLogImpression() = 0;

    // Trigger when the primary button in the UI (if exists) is pressed.
    virtual void OnPrimaryButtonPressed() = 0;

    // Trigger when the secondary button in the UI (if exists) is pressed.
    virtual void OnSecondaryButtonPressed() = 0;

    // Trigger when the close button in the UI (if exists) is pressed.
    virtual void OnCloseButtonPressed() = 0;

    // Trigger when the UI is pressed.
    // NOTE: Any button press could dismiss the UI. And the UI could auto
    // dismiss after some time.
    // TODO: b/330956316 - Log dismissal by reasons, e.g. the nudge is dismissed
    // automatically.
    virtual void OnUiDismissed() = 0;
  };

  UiActionPerformer();
  ~UiActionPerformer() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  void NotifyReadyToLogImpression();
  void NotifyUiDismissed();
  void NotifyPrimaryButtonPressed();
  void NotifySecondaryButtonPressed();
  void NotifyCloseButtonPressed();

 private:
  base::ObserverList<Observer> observers_;
};

#endif  // CHROME_BROWSER_ASH_GROWTH_SHOW_NUDGE_ACTION_PERFORMER_H_
