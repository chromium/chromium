// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CAMERA_CAMERA_GENERAL_SURVEY_HANDLER_H_
#define CHROME_BROWSER_ASH_CAMERA_CAMERA_GENERAL_SURVEY_HANDLER_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/scoped_observation_traits.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/hats/hats_config.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"

namespace ash {

// Used to show a Happiness Tracking Survey after the user finished
// using camera after being used for |min_usage_duration_|.
// The user is considered "finished" using the camera if the camera remains
// closed for |trigger_delay_|.
class CameraGeneralSurveyHandler : public media::CameraActiveClientObserver {
 public:
  struct HardwareInfo {
    std::string board;
    std::string model;
  };

  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Adds the survey handler as an observer of CameraActiveClientObserver.
    virtual void AddActiveCameraClientObserver(
        media::CameraActiveClientObserver* observer) = 0;

    // Removes the survey handler as an observer of CameraActiveClientObserver.
    virtual void RemoveActiveCameraClientObserver(
        media::CameraActiveClientObserver* observer) = 0;

    // Loads the config then keeps it internally.
    virtual void LoadConfig() = 0;

    // Checks whether the device met all conditions to participate the survey.
    virtual bool ShouldShowSurvey() const = 0;

    // Triggers the survey invitation notification,
    // in which when open will show the camera survey.
    virtual void ShowSurvey() = 0;
  };

  CameraGeneralSurveyHandler();
  // This constructor is used for testing
  CameraGeneralSurveyHandler(bool is_enabled,
                             std::unique_ptr<Delegate> delegate,
                             base::TimeDelta min_usage_duration,
                             base::TimeDelta trigger_delay);
  CameraGeneralSurveyHandler(const CameraGeneralSurveyHandler&) = delete;
  CameraGeneralSurveyHandler& operator=(const CameraGeneralSurveyHandler&) =
      delete;
  ~CameraGeneralSurveyHandler() override;

  void OnActiveClientChange(
      cros::mojom::CameraClientType type,
      bool is_new_active_client,
      const base::flat_set<std::string>& active_device_ids) override;

 private:
  void TriggerSurvey();

  // Describes how long a camera must remain open to trigger the survey show.
  const base::TimeDelta min_usage_duration_;

  // Describes delay between the camera close event and the survey show.
  const base::TimeDelta trigger_delay_;

  // Timer for implementation of "delayed" survey show.
  // This timer will be reset if the user reopens a camera.
  base::OneShotTimer timer_;

  // The last time the event OnActiveClientChange was fired with |is_active| =
  // true.
  base::TimeTicks open_at_;

  const bool is_enabled_;
  const std::unique_ptr<Delegate> delegate_;
  bool has_triggered_ = false;
  base::ScopedObservation<Delegate, media::CameraActiveClientObserver>
      camera_observer_{this};
  base::WeakPtrFactory<CameraGeneralSurveyHandler> weak_ptr_factory_{this};
};

}  // namespace ash

namespace base {

template <>
struct ScopedObservationTraits<ash::CameraGeneralSurveyHandler::Delegate,
                               media::CameraActiveClientObserver> {
  static void AddObserver(ash::CameraGeneralSurveyHandler::Delegate* source,
                          media::CameraActiveClientObserver* observer) {
    source->AddActiveCameraClientObserver(observer);
  }
  static void RemoveObserver(ash::CameraGeneralSurveyHandler::Delegate* source,
                             media::CameraActiveClientObserver* observer) {
    source->RemoveActiveCameraClientObserver(observer);
  }
};

}  // namespace base

#endif  // CHROME_BROWSER_ASH_CAMERA_CAMERA_GENERAL_SURVEY_HANDLER_H_
