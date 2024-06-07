// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_RESTORE_PINE_CONTROLLER_H_
#define ASH_WM_WINDOW_RESTORE_PINE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/wm/overview/overview_observer.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

struct InformedRestoreContentsData;

// Controls showing the pine dialog. Receives data from the full restore
// service.
class ASH_EXPORT PineController : public OverviewObserver,
                                  public wm::ActivationChangeObserver {
 public:
  PineController();
  PineController(const PineController&) = delete;
  PineController& operator=(const PineController&) = delete;
  ~PineController() override;

  InformedRestoreContentsData* contents_data() { return contents_data_.get(); }
  const InformedRestoreContentsData* contents_data() const {
    return contents_data_.get();
  }

  // Shows the onboarding message. If `restore_on` is true, only the
  // "Continue" button will be shown. Otherwise shows both buttons.
  void MaybeShowInformedRestoreOnboarding(bool restore_on);

  // Starts an overview session with the pine contents view if certain
  // conditions are met. Uses fake for testing only data.
  // TODO(hewer): Remove this temporary function.
  void MaybeStartPineOverviewSessionDevAccelerator();

  // Starts an overview session with the pine contents view if certain
  // conditions are met. Triggered by developer accelerator or on login.
  // `contents_data` is stored in `contents_data_` as we will support
  // re-entering the pine session if no windows have opened for example. It will
  // be populated with a screenshot if possible and then referenced when an
  // overview pine session is entered.
  void MaybeStartPineOverviewSession(
      std::unique_ptr<InformedRestoreContentsData> contents_data);

  // Ends the overview session if it is active and deletes
  // `contents_data_`.
  void MaybeEndPineOverviewSession();

  // OverviewObserver:
  void OnOverviewModeEnding(OverviewSession* overview_session) override;
  void OnOverviewModeEndingAnimationComplete(bool canceled) override;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

 private:
  friend class InformedRestoreTestApi;
  FRIEND_TEST_ALL_PREFIXES(PineTest, OnboardingMetrics);

  // Callback function for when the pine image is finished decoding.
  void OnPineImageDecoded(base::TimeTicks start_time,
                          const gfx::ImageSkia& pine_image);

  void StartPineOverviewSession();

  // Called when the accept or cancel button in the onboarding dialog is
  // pressed.
  void OnOnboardingAcceptPressed(bool restore_on);
  void OnOnboardingCancelPressed();

  // True if overview was in informed restore session, up until the overview
  // animation is ended.
  bool in_informed_restore_ = false;

  // The first-time experience onboarding dialog.
  views::UniqueWidgetPtr onboarding_widget_;

  // Stores the data needed to display the dialog. Created on login, and
  // deleted after the user interacts with the dialog. If the user exits
  // overview, this will persist until a window is opened.
  std::unique_ptr<InformedRestoreContentsData> contents_data_;

  base::ScopedObservation<wm::ActivationClient, wm::ActivationChangeObserver>
      activation_change_observation_{this};

  base::WeakPtrFactory<PineController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_RESTORE_PINE_CONTROLLER_H_
