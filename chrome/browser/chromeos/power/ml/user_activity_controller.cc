// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/ml/user_activity_controller.h"

#include "base/feature_list.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/viz/public/mojom/compositing/video_detector_observer.mojom.h"
#include "ui/aura/env.h"
#include "ui/compositor/compositor.h"

namespace chromeos {
namespace power {
namespace ml {

UserActivityController::UserActivityController() {
  if (!base::FeatureList::IsEnabled(features::kSmartDim))
    return;

  chromeos::PowerManagerClient* power_manager_client =
      chromeos::PowerManagerClient::Get();
  DCHECK(power_manager_client);
  ui::UserActivityDetector* detector = ui::UserActivityDetector::Get();
  DCHECK(detector);
  session_manager::SessionManager* session_manager =
      session_manager::SessionManager::Get();
  DCHECK(session_manager);

  // TODO(jiameng): both IdleEventNotifier and UserActivityManager implement
  // viz::mojom::VideoDetectorObserver. We should refactor the code to create
  // one shared video detector observer class.
  mojo::PendingRemote<viz::mojom::VideoDetectorObserver>
      video_observer_idle_notifier;
  idle_event_notifier_ = std::make_unique<IdleEventNotifier>(
      power_manager_client, detector,
      video_observer_idle_notifier.InitWithNewPipeAndPassReceiver());
  aura::Env::GetInstance()
      ->context_factory_private()
      ->GetHostFrameSinkManager()
      ->AddVideoDetectorObserver(std::move(video_observer_idle_notifier));

  mojo::PendingRemote<viz::mojom::VideoDetectorObserver>
      video_observer_user_logger;
  user_activity_manager_ = std::make_unique<UserActivityManager>(
      &user_activity_ukm_logger_, detector, power_manager_client,
      session_manager,
      video_observer_user_logger.InitWithNewPipeAndPassReceiver(),
      chromeos::ChromeUserManager::Get(), &smart_dim_model_);
  aura::Env::GetInstance()
      ->context_factory_private()
      ->GetHostFrameSinkManager()
      ->AddVideoDetectorObserver(std::move(video_observer_user_logger));
}

UserActivityController::~UserActivityController() = default;

void UserActivityController::ShouldDeferScreenDim(
    base::OnceCallback<void(bool)> callback) {
  user_activity_manager_->UpdateAndGetSmartDimDecision(
      idle_event_notifier_->GetActivityDataAndReset(), std::move(callback));
}

}  // namespace ml
}  // namespace power
}  // namespace chromeos
