// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "camera_general_survey_handler.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "chrome/browser/ash/hats/hats_config.h"
#include "chrome/browser/ash/hats/hats_notification_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom-shared.h"

namespace ash {

constexpr base::TimeDelta kMinCameraOpenDurationForSurvey = base::Seconds(15);
constexpr base::TimeDelta kCameraSurveyTriggerDelay = base::Seconds(5);

class CameraGeneralSurveyHandlerDelegate
    : public CameraGeneralSurveyHandler::Delegate {
 public:
  CameraGeneralSurveyHandlerDelegate() = default;
  ~CameraGeneralSurveyHandlerDelegate() override = default;
  CameraGeneralSurveyHandlerDelegate(
      const CameraGeneralSurveyHandlerDelegate&) = delete;
  CameraGeneralSurveyHandlerDelegate& operator=(
      const CameraGeneralSurveyHandlerDelegate&) = delete;

  void AddActiveCameraClientObserver(
      media::CameraActiveClientObserver* observer) override {
    media::CameraHalDispatcherImpl::GetInstance()->AddActiveClientObserver(
        observer);
  }

  void RemoveActiveCameraClientObserver(
      media::CameraActiveClientObserver* observer) override {
    media::CameraHalDispatcherImpl::GetInstance()->RemoveActiveClientObserver(
        observer);
  }

  bool ShouldShowSurvey() const override {
    return HatsNotificationController::ShouldShowSurveyToProfile(
        ProfileManager::GetActiveUserProfile(), kHatsGeneralCameraSurvey);
  }

  void ShowSurvey() override {
    base::SysInfo::GetHardwareInfo(base::BindOnce(
        &CameraGeneralSurveyHandlerDelegate::OnHardwareInfoFetched,
        weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  void OnHardwareInfoFetched(base::SysInfo::HardwareInfo info) {
    Profile* profile = ProfileManager::GetActiveUserProfile();
    base::flat_map<std::string, std::string> survey_data = {
        {"board", base::SysInfo::GetLsbReleaseBoard()}, {"model", info.model}};
    hats_notification_controller_ =
        base::MakeRefCounted<HatsNotificationController>(
            profile, kHatsGeneralCameraSurvey, survey_data);
  }

  scoped_refptr<HatsNotificationController> hats_notification_controller_;
  base::WeakPtrFactory<CameraGeneralSurveyHandlerDelegate> weak_ptr_factory_{
      this};
};

CameraGeneralSurveyHandler::CameraGeneralSurveyHandler()
    : CameraGeneralSurveyHandler(
          base::FeatureList::IsEnabled(kHatsGeneralCameraSurvey.feature),
          std::make_unique<CameraGeneralSurveyHandlerDelegate>(),
          kMinCameraOpenDurationForSurvey,
          kCameraSurveyTriggerDelay) {}

CameraGeneralSurveyHandler::CameraGeneralSurveyHandler(
    bool is_enabled,
    std::unique_ptr<Delegate> delegate,
    base::TimeDelta min_usage_duration,
    base::TimeDelta trigger_delay)
    : min_usage_duration_(min_usage_duration),
      trigger_delay_(trigger_delay),
      is_enabled_(is_enabled),
      delegate_(std::move(delegate)) {
  if (!is_enabled_) {
    VLOG(1) << "General camera survey feature is not enabled";
    return;
  }
  camera_observer_.Observe(delegate_.get());
}

CameraGeneralSurveyHandler::~CameraGeneralSurveyHandler() = default;

void CameraGeneralSurveyHandler::TriggerSurvey() {
  delegate_->ShowSurvey();
  has_triggered_ = true;
}

void CameraGeneralSurveyHandler::OnActiveClientChange(
    cros::mojom::CameraClientType type,
    bool is_new_active_client,
    const base::flat_set<std::string>& active_device_ids) {
  if (has_triggered_) {
    return;
  }
  if (is_new_active_client) {
    // Event: a camera is open.
    open_at_ = base::TimeTicks::Now();

    // Cancel showing survey if a camera is reopened.
    timer_.Stop();
  } else if (!is_new_active_client && active_device_ids.empty()) {
    // Event: the last open camera is now closed.
    auto now = base::TimeTicks::Now();
    auto elapsed = now - open_at_;
    if (elapsed < min_usage_duration_) {
      return;
    }
    if (delegate_->ShouldShowSurvey()) {
      timer_.Start(FROM_HERE, trigger_delay_,
                   base::BindOnce(&CameraGeneralSurveyHandler::TriggerSurvey,
                                  weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

}  // namespace ash
