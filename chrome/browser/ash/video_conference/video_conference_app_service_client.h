// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_VIDEO_CONFERENCE_VIDEO_CONFERENCE_APP_SERVICE_CLIENT_H_
#define CHROME_BROWSER_ASH_VIDEO_CONFERENCE_VIDEO_CONFERENCE_APP_SERVICE_CLIENT_H_

#include <map>
#include <string>

#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chrome/browser/chromeos/video_conference/video_conference_manager_client_common.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/instance_registry.h"

namespace apps {
class AppRegistryCache;
class InstanceRegistry;
}  // namespace apps

namespace ukm {
class UkmRecorder;
}

namespace video_conference {
class VideoConferenceUkmHelper;
}

namespace ash {

// VideoConferenceAppServiceClient is a client class for CrOS
// videoconferencing. It detects the launching/closing/media-capturing actions
// from apps through AppService and notifies VideoConferenceManagerAsh.
class VideoConferenceAppServiceClient
    : public crosapi::mojom::VideoConferenceManagerClient,
      public apps::AppCapabilityAccessCache::Observer,
      public apps::InstanceRegistry::Observer,
      public SessionObserver {
 public:
  using AppIdString = std::string;
  using VideoConferencePermissions =
      video_conference::VideoConferencePermissions;

  // AppState records information that is required for VideoConferenceManagerAsh
  // to show correct icons.
  struct AppState {
    // Used for uniquely identifying an App in VideoConferenceManagerAsh.
    base::UnguessableToken token;
    base::Time last_activity_time;
    bool is_capturing_microphone = false;
    bool is_capturing_camera = false;
  };

  VideoConferenceAppServiceClient();

  VideoConferenceAppServiceClient(const VideoConferenceAppServiceClient&) =
      delete;
  VideoConferenceAppServiceClient& operator=(
      const VideoConferenceAppServiceClient&) = delete;

  ~VideoConferenceAppServiceClient() override;

  // crosapi::mojom::VideoConferenceManagerClient overrides.
  void GetMediaApps(GetMediaAppsCallback callback) override;
  void ReturnToApp(const base::UnguessableToken& token,
                   ReturnToAppCallback callback) override;
  void SetSystemMediaDeviceStatus(
      crosapi::mojom::VideoConferenceMediaDevice device,
      bool disabled,
      SetSystemMediaDeviceStatusCallback callback) override;
  void StopAllScreenShare() override;

  // apps::AppCapabilityAccessCache::Observer overrides.
  void OnCapabilityAccessUpdate(
      const apps::CapabilityAccessUpdate& update) override;
  void OnAppCapabilityAccessCacheWillBeDestroyed(
      apps::AppCapabilityAccessCache* cache) override;

  // apps::InstanceRegistry::Observer overrides.
  void OnInstanceUpdate(const apps::InstanceUpdate& update) override;
  void OnInstanceRegistryWillBeDestroyed(
      apps::InstanceRegistry* cache) override;

  // SessionObserver overrides.
  void OnSessionStateChanged(session_manager::SessionState state) override;

 private:
  friend class VideoConferenceAppServiceClientTest;

  // Returns current VideoConferenceAppServiceClient for testing purpose.
  static VideoConferenceAppServiceClient* GetForTesting();

  // Returns the name of the app with `app_id`.
  std::string GetAppName(const AppIdString& app_id);

  // Returns the current camera/microphone permission status for `app_id`.
  VideoConferencePermissions GetAppPermission(const AppIdString& app_id);

  // Returns the AppType of `app_id`.
  apps::AppType GetAppType(const AppIdString& app_id);

  // Returns AppState of `app_id`; adds if doesn't exist yet.
  AppState& GetOrAddAppState(const AppIdString& app_id);

  // Removes `app_id` from `id_to_app_state_` if there is no running instance
  // for it.
  void MaybeRemoveApp(const AppIdString& app_id);

  // Calculates a new `crosapi::mojom::VideoConferenceMediaUsageStatus` from all
  // current VC apps and notifies the manager if a field has changed.
  void HandleMediaUsageUpdate();

  // These registries are used for observing app behaviors.
  raw_ptr<apps::InstanceRegistry> instance_registry_;
  raw_ptr<apps::AppRegistryCache> app_registry_;
  raw_ptr<apps::AppCapabilityAccessCache> capability_cache_;

  // Unique id associated with this client. It is used by the VcManager to
  // identify clients.
  const base::UnguessableToken client_id_;

  // Only used for testing purpose.
  raw_ptr<ukm::UkmRecorder> test_ukm_recorder_ = nullptr;

  // Current status_ aggregated from all apps in `id_to_app_state_`.
  crosapi::mojom::VideoConferenceMediaUsageStatusPtr status_;

  // The following two fields are true if the camera/microphone is system-wide
  // software disabled OR disabled via a hardware switch.
  bool camera_system_disabled_{false};
  bool microphone_system_disabled_{false};

  // This records a list of AppState; each represents a video conference app.
  std::map<AppIdString, AppState> id_to_app_state_;

  std::map<AppIdString,
           std::unique_ptr<video_conference::VideoConferenceUkmHelper>>
      id_to_ukm_hepler_;

  base::ScopedObservation<SessionController, SessionObserver>
      session_observation_{this};

  base::ScopedObservation<apps::InstanceRegistry,
                          apps::InstanceRegistry::Observer>
      instance_registry_observation_{this};

  base::ScopedObservation<apps::AppCapabilityAccessCache,
                          apps::AppCapabilityAccessCache::Observer>
      app_capability_observation_{this};

  base::WeakPtrFactory<VideoConferenceAppServiceClient> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_VIDEO_CONFERENCE_VIDEO_CONFERENCE_APP_SERVICE_CLIENT_H_
