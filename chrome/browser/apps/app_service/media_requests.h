// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_MEDIA_REQUESTS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_MEDIA_REQUESTS_H_

#include <map>
#include <optional>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "content/public/browser/media_request_state.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"

namespace apps {

struct AccessingRequest {
  AccessingRequest(std::optional<bool> camera, std::optional<bool> microphone);
  AccessingRequest(const AccessingRequest&) = delete;
  AccessingRequest& operator=(const AccessingRequest&) = delete;
  AccessingRequest(AccessingRequest&&);
  AccessingRequest& operator=(AccessingRequest&&);
  ~AccessingRequest();

  std::optional<bool> camera;
  std::optional<bool> microphone;
};

// MediaRequests tracks the media usage for each app (e.g. accessing camera,
// microphone) on a per-WebContents basis.
class MediaRequests {
 public:
  MediaRequests();
  ~MediaRequests();

  MediaRequests(const MediaRequests&) = delete;
  MediaRequests& operator=(const MediaRequests&) = delete;

  // Updates media request state to indicate that the app represented by
  // `app_id` is/isn't accessing the microphone in the given `web_contents`.
  // Returns an AccessingRequest with the updated tracked state for the app.
  AccessingRequest UpdateMicrophoneState(
      const std::string& app_id,
      const content::WebContents* web_contents,
      bool is_accessing_microphone);

  // Updates media request state to indicate that the app represented by
  // `app_id` is/isn't accessing the camera in the given `web_contents`. Returns
  // an AccessingRequest with the updated tracked state for the app.
  AccessingRequest UpdateCameraState(const std::string& app_id,
                                     const content::WebContents* web_contents,
                                     bool is_accessing_camera);

  // Removes requests in |app_id_to_web_contents_for_camera_| and
  // |app_id_to_web_contents_for_microphone_| for the given |app_id|. If there
  // are media accessing requests for |app_id|, returns false for
  // AccessingRequest.camera or AccessingRequest.microphone.
  AccessingRequest RemoveRequests(const std::string& app_id);

 private:
  // Web contents which are accessing the cemera or microphone.
  using WebContents =
      std::set<raw_ptr<const content::WebContents, SetExperimental>>;

  // Maps one app id to a set of web contents.
  using AppIdToWebContents = std::map<std::string, WebContents>;

  std::optional<bool> MaybeAddRequest(
      const std::string& app_id,
      const content::WebContents* web_contents,
      AppIdToWebContents& app_id_to_web_contents);

  std::optional<bool> MaybeRemoveRequest(
      const std::string& app_id,
      const content::WebContents* web_contents,
      AppIdToWebContents& app_id_to_web_contents);

  std::optional<bool> MaybeRemoveRequest(
      const std::string& app_id,
      AppIdToWebContents& app_id_to_web_contents);

  // Maps one app id to a set of web contents which are accessing the camera.
  AppIdToWebContents app_id_to_web_contents_for_camera_;

  // Maps one app id to a set of web contents which are accessing the
  // microphone.
  AppIdToWebContents app_id_to_web_contents_for_microphone_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_MEDIA_REQUESTS_H_
