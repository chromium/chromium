// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CAMERA_CAMERA_EFFECTS_CONTROLLER_H_
#define ASH_SYSTEM_CAMERA_CAMERA_EFFECTS_CONTROLLER_H_

#include <utility>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/camera/autozoom_observer.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_delegate.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"
#include "media/capture/video/chromeos/mojom/effects_pipeline.mojom.h"

class PrefRegistrySimple;
class PrefService;
class PrefChangeRegistrar;

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace ash {

enum class VcEffectId;

// CameraEffectsController is the interface for any object in ash to
// enable/change camera effects.
class ASH_EXPORT CameraEffectsController : public AutozoomObserver,
                                           public media::CameraEffectObserver,
                                           public SessionObserver,
                                           public VcEffectsDelegate {
 public:
  // Enum that represents the value persisted  to `prefs::kBackgroundBlur`,
  // which is the "ultimate source of truth" for the background blur setting.
  // WARNING: This enum and `prefs::kBackgroundBlur` should always be in sync.
  // Any changes made to this enum or `prefs::kBackgroundBlur` should also be
  // reflected in the other place.
  enum BackgroundBlurPrefValue {
    kOff = -1,
    kLowest = 0,
    kLight = 1,
    kMedium = 2,
    kHeavy = 3,
    kMaximum = 4,
  };

  // This enum contains all the state of the background blur effect. This enum
  // is used for metrics collection (we cannot use `BackgroundBlurPrefValue`
  // since `base::UmaHistogramEnumeration` cannot take a negative value for
  // an enum). Note to keep in sync with enum in
  // tools/metrics/histograms/enums.xml.
  enum class BackgroundBlurState {
    kOff = 0,
    kLowest = 1,
    kLight = 2,
    kMedium = 3,
    kHeavy = 4,
    kMaximum = 5,
    kMaxValue = kMaximum
  };

  // Information of a single background image file used in the ui.
  struct BackgroundImageInfo {
    base::Time creation_time;
    base::Time last_accessed;
    std::string basename;
    std::string jpeg_bytes;
  };

  CameraEffectsController();

  CameraEffectsController(const CameraEffectsController&) = delete;
  CameraEffectsController& operator=(const CameraEffectsController&) = delete;

  ~CameraEffectsController() override;

  // Returns 'true' if UI controls for `effect` are available to the user,
  // 'false' otherwise.
  bool IsEffectControlAvailable(
      cros::mojom::CameraEffect effect = cros::mojom::CameraEffect::kNone);

  // Returns currently applied camera effects.
  // Should only be called after user logs in.
  cros::mojom::EffectsConfigPtr GetCameraEffects();

  // Called inside ash/ash_prefs.cc to register related prefs.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Sets an image as the camera background.
  // The `relative_path` is relative to `camera_background_img_dir_` and the
  // file has to exist for the effect to work.
  void SetBackgroundImage(const base::FilePath& relative_path);

  // Saves the `jpeg_bytes` as an image file and apply that as camera
  // background.
  void SetBackgroundImageFromContent(std::string&& jpeg_bytes);

  // Removes `basename` from the camera background directory; remove background
  // effect if the same file is used as camera background right now.
  void RemoveBackgroundImage(const base::FilePath& basename);

  // Gets `number_of_images` recently used camera background images, and calls
  // the `callback` on the returned list.
  void GetRecentlyUsedBackgroundImages(
      const int number_of_images,
      base::OnceCallback<void(const std::vector<BackgroundImageInfo>&)>
          callback);

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // VcEffectsDelegate:
  std::optional<int> GetEffectState(VcEffectId effect_id) override;
  void OnEffectControlActivated(VcEffectId effect_id,
                                std::optional<int> state) override;
  void RecordMetricsForSetValueEffectOnClick(VcEffectId effect_id,
                                             int state_value) const override;
  void RecordMetricsForSetValueEffectOnStartup(VcEffectId effect_id,
                                               int state_value) const override;

  // media::CameraEffectObserver:
  void OnCameraEffectChanged(
      const cros::mojom::EffectsConfigPtr& new_effects) final;

  void bypass_set_camera_effects_for_testing(bool in_testing_mode) {
    in_testing_mode_ = in_testing_mode;
  }

  void set_camera_background_img_dir_for_testing(
      const base::FilePath& camera_background_img_dir) {
    camera_background_img_dir_ = camera_background_img_dir;
  }

  void set_camera_background_run_dir_for_testing(
      const base::FilePath& camera_background_run_dir) {
    camera_background_run_dir_ = camera_background_run_dir;
  }

 private:
  // AutozoomObserver:
  void OnAutozoomControlEnabledChanged(bool enabled) override;

  // Returns the segmentation model that should be used in the effects pipeline
  // based on the value of the feature flag.
  cros::mojom::SegmentationModel GetSegmentationModelType();

  // SetCameraEffects camera effects with `config`.
  void SetCameraEffects(cros::mojom::EffectsConfigPtr config,
                        bool is_initialization);

  // Called only after copying background images to
  // `camera_background_run_dir_`. If `copy_succeeded`, then `new_config` will
  // be applied. If `copy_succeeded` is false, but `is_initialization`, then we
  // will still apply other effects except background replace.
  void OnCopyBackgroundImageFileComplete(
      cros::mojom::EffectsConfigPtr new_config,
      bool is_initialization,
      bool copy_succeeded);

  // Constructs EffectsConfigPtr from prefs.
  cros::mojom::EffectsConfigPtr GetEffectsConfigFromPref();

  // Update prefs with the value in `config`.
  void SetEffectsConfigToPref(cros::mojom::EffectsConfigPtr config);

  // Performs any initializations needed for effects whose controls are
  // exposed via the UI.
  void InitializeEffectControls();

  // Adds a `std::unique_ptr<VcEffectState>` to `effect`, where `effect` is
  // assumed to be that of camera background blur.
  void AddBackgroundBlurStateToEffect(VcHostedEffect* effect,
                                      const gfx::VectorIcon& icon,
                                      int state_value,
                                      int string_id);

  // A helper for easier binding.
  void SetCameraEffectsInCameraHalDispatcherImpl(
      cros::mojom::EffectsConfigPtr config);

  // Used to bypass the CameraHalDispatcherImpl::SetCameraEffects for
  // testing purpose.
  bool in_testing_mode_ = false;

  // Directory that stores the camera background images.
  base::FilePath camera_background_img_dir_;

  // Directory that stores the background images for the camera module to use.
  base::FilePath camera_background_run_dir_;

  // Used for pref registration.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // This task runner is used to ensure `current_effects_` is always accessed
  // from the same thread.
  const scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  // This task runner is used to run io work.
  const scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  // Records current effects that is applied to camera hal server.
  cros::mojom::EffectsConfigPtr current_effects_;

  base::ScopedObservation<SessionController, SessionObserver>
      session_observation_{this};

  base::WeakPtrFactory<CameraEffectsController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_CAMERA_CAMERA_EFFECTS_CONTROLLER_H_
