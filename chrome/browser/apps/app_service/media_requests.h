// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_MEDIA_REQUESTS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_MEDIA_REQUESTS_H_

#include <map>
#include <set>
#include <string>

#include "content/public/browser/media_request_state.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"

namespace apps {

struct AccessingRequest {
  AccessingRequest(absl::optional<bool> camera,
                   absl::optional<bool> microphone);
  AccessingRequest(const AccessingRequest&) = delete;
  AccessingRequest& operator=(const AccessingRequest&) = delete;
  AccessingRequest(AccessingRequest&&);
  AccessingRequest& operator=(AccessingRequest&&);
  ~AccessingRequest();

  absl::optional<bool> camera;
  absl::optional<bool> microphone;
};

// MediaRequests records the media access requests for each app, e.g. accessing
// camera, microphone.
class MediaRequests {
 public:
  MediaRequests();
  ~MediaRequests();

  MediaRequests(const MediaRequests&) = delete;
  MediaRequests& operator=(const MediaRequests&) = delete;

  // Returns true if there is no existing access request of both camera and
  // microphone for |app_id| and |web_contents|, and |state| is a new request.
  // Otherwise, return false.
  bool IsNewRequest(const std::string& app_id,
                    const content::WebContents* web_contents,
                    const content::MediaRequestState state);

  // Updates |app_id_to_web_contents_for_camera_| and
  // |app_id_to_web_contents_for_microphone_| to record the media accessing
  // requests for |app_id|. Returns the update result AccessingRequest.
  AccessingRequest UpdateRequests(const std::string& app_id,
                                  const content::WebContents* web_contents,
                                  blink::mojom::MediaStreamType stream_type,
                                  const content::MediaRequestState state);

  // Removes requests in |app_id_to_web_contents_for_camera_| and
  // |app_id_to_web_contents_for_microphone_| for the given |app_id|. If there
  // are media accessing requests for |app_id|, returns false for
  // AccessingRequest.camera or AccessingRequest.microphone.
  AccessingRequest RemoveRequests(const std::string& app_id);

  // Invoked when |web_contents| is being destroyed. Removes requests in
  // |app_id_to_web_contents_for_camera_| and
  // |app_id_to_web_contents_for_microphone_| for the given |app_id| and
  // |web_contents|. If there are media accessing requests for |app_id|, returns
  // false for AccessingRequest.camera or AccessingRequest.microphone.
  AccessingRequest OnWebContentsDestroyed(
      const std::string& app_id,
      const content::WebContents* web_contents);

 private:
  bool HasRequest(
      const std::string& app_id,
      const content::WebContents* web_contents,
      const std::map<std::string, std::set<const content::WebContents*>>&
          app_id_to_web_contents);

  absl::optional<bool> MaybeAddRequest(
      const std::string& app_id,
      const content::WebContents* web_contents,
      std::map<std::string, std::set<const content::WebContents*>>&
          app_id_to_web_contents);

  absl::optional<bool> MaybeRemoveRequest(
      const std::string& app_id,
      const content::WebContents* web_contents,
      std::map<std::string, std::set<const content::WebContents*>>&
          app_id_to_web_contents);

  absl::optional<bool> MaybeRemoveRequest(
      const std::string& app_id,
      std::map<std::string, std::set<const content::WebContents*>>&
          app_id_to_web_contents);

  // Maps one app id to a set of web contents which are accessing the cemera.
  // Web contents pointer is being used as a key and nothing else, and the
  // pointer should not be dereferenced.
  std::map<std::string, std::set<const content::WebContents*>>
      app_id_to_web_contents_for_camera_;

  // Maps one app id to a set of web contents which are accessing the
  // microphone. Web contents pointer is being used as a key and nothing else,
  // and the pointer should not be dereferenced.
  std::map<std::string, std::set<const content::WebContents*>>
      app_id_to_web_contents_for_microphone_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_MEDIA_REQUESTS_H_
