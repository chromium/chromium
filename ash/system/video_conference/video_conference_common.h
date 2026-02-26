// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_COMMON_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_COMMON_H_

#include <optional>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chromeos/crosapi/mojom/video_conference.mojom-forward.h"

namespace ash {

// Struct containing the id and new title for a VC app whose title updated.
struct ASH_EXPORT TitleChangeInfo {
  TitleChangeInfo();
  TitleChangeInfo(const TitleChangeInfo&);
  TitleChangeInfo& operator=(const TitleChangeInfo&);
  TitleChangeInfo(TitleChangeInfo&&) noexcept;
  TitleChangeInfo& operator=(TitleChangeInfo&&) noexcept;
  ~TitleChangeInfo();

  // Unique id corresponding to a VC web app.
  base::UnguessableToken id;

  // The VC app's new title.
  std::u16string new_title;
};

enum class VideoConferenceAppUpdate {
  kNone,
  kAppAdded,
  kAppRemoved,
};

// Useful notifications from clients. Intended mainly for the VC tray.
struct ASH_EXPORT VideoConferenceClientUpdate {
  explicit VideoConferenceClientUpdate(VideoConferenceAppUpdate update_type);
  VideoConferenceClientUpdate();
  VideoConferenceClientUpdate(const VideoConferenceClientUpdate&);
  VideoConferenceClientUpdate& operator=(const VideoConferenceClientUpdate&);
  VideoConferenceClientUpdate(VideoConferenceClientUpdate&&) noexcept;
  VideoConferenceClientUpdate& operator=(
      VideoConferenceClientUpdate&&) noexcept;
  ~VideoConferenceClientUpdate();

  // Client just added or removed a new VC app.
  VideoConferenceAppUpdate added_or_removed_app =
      VideoConferenceAppUpdate::kNone;

  // Title change info. Only present if this client update was
  // triggered by a title change.
  std::optional<TitleChangeInfo> title_change_info;
};

constexpr int kVideoConferenceBubbleHorizontalPadding = 16;

const int kReturnToAppIconSize = 20;

// The duration for the gradient animation on the Image and Create with AI
// buttons.
const base::TimeDelta kGradientAnimationDuration = base::Milliseconds(3120);

// This struct provides aggregated attributes of media apps
// from one or more clients.
struct VideoConferenceMediaState {
  // At least one media app is running on the client(s).
  bool has_media_app = false;
  // At least one media app has camera permission on the client(s).
  bool has_camera_permission = false;
  // At least one media app has microphone permission on the client(s).
  bool has_microphone_permission = false;
  // At least one media app is capturing the camera on the client(s).
  bool is_capturing_camera = false;
  // At least one media app is capturing the microphone on the client(s).
  bool is_capturing_microphone = false;
  // At least one media app is capturing the screen on the client(s).
  bool is_capturing_screen = false;

  bool operator==(const VideoConferenceMediaState& other) const;
};

// Aggregated media usage status for a client.
struct ASH_EXPORT VideoConferenceMediaUsageStatus {
  explicit VideoConferenceMediaUsageStatus(
      const base::UnguessableToken& client_id);
  VideoConferenceMediaUsageStatus(const VideoConferenceMediaUsageStatus&);
  VideoConferenceMediaUsageStatus& operator=(
      const VideoConferenceMediaUsageStatus&);
  VideoConferenceMediaUsageStatus(VideoConferenceMediaUsageStatus&&) noexcept;
  VideoConferenceMediaUsageStatus& operator=(
      VideoConferenceMediaUsageStatus&&) noexcept;
  ~VideoConferenceMediaUsageStatus();

  base::UnguessableToken client_id;
  VideoConferenceMediaState state;

  bool operator==(const VideoConferenceMediaUsageStatus& other) const;
};

// Represents the media devices that can be captured by a video conferencing
// app.
enum class VideoConferenceMediaDevice {
  kMicrophone,
  kCamera,
};

// Client interface implemented by Ash and Chrome clients to interact with the
// VideoConferenceManagerAsh.
class ASH_EXPORT VideoConferenceManagerClient {
 public:
  // TODO(crbug.com/365741912, crbug.com/365902693): In a later CL, drop these
  // callbacks and return the result directly.
  using GetMediaAppsCallback = base::OnceCallback<void(
      std::vector<crosapi::mojom::VideoConferenceMediaAppInfoPtr>)>;
  using ReturnToAppCallback = base::OnceCallback<void(bool)>;
  using SetSystemMediaDeviceStatusCallback = base::OnceCallback<void(bool)>;

  virtual ~VideoConferenceManagerClient() = default;

  virtual void GetMediaApps(GetMediaAppsCallback callback) = 0;
  virtual void ReturnToApp(const base::UnguessableToken& id,
                           ReturnToAppCallback callback) = 0;
  virtual void SetSystemMediaDeviceStatus(
      VideoConferenceMediaDevice device,
      bool enabled,
      SetSystemMediaDeviceStatusCallback callback) = 0;
};

// This class defines the public interfaces of VideoConferenceManagerAsh exposed
// to VideoConferenceTrayController. Although these public functions look
// identical to VideoConferenceManagerClient, we should not use
// VideoConferenceManagerClient here; because they represent different concepts.
// The signal will be passed from VideoConferenceTrayController to
// VideoConferenceManagerAsh to VideoConferenceManagerClient.
class VideoConferenceManagerBase {
 public:
  using MediaApps = std::vector<crosapi::mojom::VideoConferenceMediaAppInfoPtr>;
  // Gets all media apps from VideoConferenceManagerAsh and runs the callback on
  // that.
  virtual void GetMediaApps(base::OnceCallback<void(MediaApps)>) = 0;

  // Calls VideoConferenceManagerAsh to return to App identified by `id`.
  virtual void ReturnToApp(const base::UnguessableToken& id) = 0;

  // Sets whether |device| is enabled at the system or hardware level.
  virtual void SetSystemMediaDeviceStatus(VideoConferenceMediaDevice device,
                                          bool enabled) = 0;

  // Called when CreateBackgroundImage button is clicked on.
  virtual void CreateBackgroundImage() = 0;

  virtual ~VideoConferenceManagerBase() = default;
};

}  // namespace ash

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_COMMON_H_
