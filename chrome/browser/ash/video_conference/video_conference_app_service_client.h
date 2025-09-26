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
#include "chrome/browser/ash/video_conference/video_conference_client_base.h"
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

class VideoConferenceManagerAsh;

// VideoConferenceAppServiceClient is a client class for CrOS
// videoconferencing. It detects the launching/closing/media-capturing actions
// from apps through AppService and notifies VideoConferenceManagerAsh.
class VideoConferenceAppServiceClient
    : public VideoConferenceClientBase,
      public apps::AppCapabilityAccessCache::Observer,
      public apps::InstanceRegistry::Observer,
      public SessionObserver {
 public:
  // The passed `video_conference_manager_ash` must outlive this instance.
  explicit VideoConferenceAppServiceClient(
      VideoConferenceManagerAsh* video_conference_manager_ash);

  VideoConferenceAppServiceClient(const VideoConferenceAppServiceClient&) =
      delete;
  VideoConferenceAppServiceClient& operator=(
      const VideoConferenceAppServiceClient&) = delete;

  ~VideoConferenceAppServiceClient() override;

  // crosapi::mojom::VideoConferenceManagerClient overrides.
  void ReturnToApp(const base::UnguessableToken& token,
                   ReturnToAppCallback callback) override;

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

 protected:
  // VideoConferenceClientBase overrides.
  std::string GetAppName(const AppIdString& app_id) override;
  VideoConferencePermissions GetAppPermission(
      const AppIdString& app_id) override;
  apps::AppType GetAppType(const AppIdString& app_id) override;

 private:
  friend class VideoConferenceAppServiceClientTest;

  // Returns current VideoConferenceAppServiceClient for testing purpose.
  static VideoConferenceAppServiceClient* GetForTesting();

  // Returns AppState of `app_id`; adds if doesn't exist yet.
  AppState& GetOrAddAppState(const AppIdString& app_id);

  // Removes `app_id` from `id_to_app_state_` if there is no running instance
  // for it.
  void MaybeRemoveApp(const AppIdString& app_id);

  // These registries are used for observing app behaviors.
  raw_ptr<apps::InstanceRegistry> instance_registry_;
  raw_ptr<apps::AppRegistryCache> app_registry_;
  raw_ptr<apps::AppCapabilityAccessCache> capability_cache_;

  // Only used for testing purpose.
  raw_ptr<ukm::UkmRecorder> test_ukm_recorder_ = nullptr;

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
