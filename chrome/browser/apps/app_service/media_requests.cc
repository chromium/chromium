// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/media_requests.h"

#include <utility>

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace apps {

AccessingRequest::AccessingRequest(absl::optional<bool> camera,
                                   absl::optional<bool> microphone)
    : camera(camera), microphone(microphone) {}

AccessingRequest::AccessingRequest(AccessingRequest&&) = default;

AccessingRequest& AccessingRequest::operator=(AccessingRequest&&) = default;

AccessingRequest::~AccessingRequest() = default;

MediaRequests::MediaRequests() = default;

MediaRequests::~MediaRequests() = default;

bool MediaRequests::IsNewRequest(const std::string& app_id,
                                 const content::WebContents* web_contents,
                                 const content::MediaRequestState state) {
  if (state != content::MEDIA_REQUEST_STATE_DONE) {
    return false;
  }

  DCHECK(web_contents);

  return !HasRequest(app_id, web_contents,
                     app_id_to_web_contents_for_camera_) &&
         !HasRequest(app_id, web_contents,
                     app_id_to_web_contents_for_microphone_);
}

AccessingRequest MediaRequests::UpdateRequests(
    const std::string& app_id,
    const content::WebContents* web_contents,
    blink::mojom::MediaStreamType stream_type,
    const content::MediaRequestState state) {
  DCHECK(web_contents);

  absl::optional<bool> accessing_camera;
  absl::optional<bool> accessing_microphone;
  if (state == content::MEDIA_REQUEST_STATE_DONE) {
    if (stream_type == blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE) {
      accessing_camera = MaybeAddRequest(app_id, web_contents,
                                         app_id_to_web_contents_for_camera_);
    }
    if (stream_type == blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE) {
      accessing_microphone = MaybeAddRequest(
          app_id, web_contents, app_id_to_web_contents_for_microphone_);
    }
  }

  if (state == content::MEDIA_REQUEST_STATE_CLOSING) {
    if (stream_type == blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE) {
      accessing_camera = MaybeRemoveRequest(app_id, web_contents,
                                            app_id_to_web_contents_for_camera_);
    }
    if (stream_type == blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE) {
      accessing_microphone = MaybeRemoveRequest(
          app_id, web_contents, app_id_to_web_contents_for_microphone_);
    }
  }

  return AccessingRequest(accessing_camera, accessing_microphone);
}

AccessingRequest MediaRequests::RemoveRequests(const std::string& app_id) {
  return AccessingRequest(
      MaybeRemoveRequest(app_id, app_id_to_web_contents_for_camera_),
      MaybeRemoveRequest(app_id, app_id_to_web_contents_for_microphone_));
}

AccessingRequest MediaRequests::OnWebContentsDestroyed(
    const std::string& app_id,
    const content::WebContents* web_contents) {
  return AccessingRequest(
      MaybeRemoveRequest(app_id, web_contents,
                         app_id_to_web_contents_for_camera_),
      MaybeRemoveRequest(app_id, web_contents,
                         app_id_to_web_contents_for_microphone_));
}

bool MediaRequests::HasRequest(
    const std::string& app_id,
    const content::WebContents* web_contents,
    const AppIdToWebContents& app_id_to_web_contents) {
  auto it = app_id_to_web_contents.find(app_id);
  if (it != app_id_to_web_contents.end() &&
      it->second.find(web_contents) != it->second.end()) {
    return true;
  }
  return false;
}

absl::optional<bool> MediaRequests::MaybeAddRequest(
    const std::string& app_id,
    const content::WebContents* web_contents,
    AppIdToWebContents& app_id_to_web_contents) {
  auto it = app_id_to_web_contents.find(app_id);
  if (it == app_id_to_web_contents.end()) {
    app_id_to_web_contents[app_id].insert(web_contents);
    // New media request for `app_id` and `web_contents`.
    return true;
  }

  auto web_contents_it = it->second.find(web_contents);
  if (web_contents_it == it->second.end()) {
    it->second.insert(web_contents);
    // New media request for `web_contents`, but not a new request for `app_id`.
    // So return nullopt, which means no change for `app_id`.
    return absl::nullopt;
  }

  // Not a new request for `app_id`. So return nullopt, which means no change
  // for`app_id`.
  return absl::nullopt;
}

absl::optional<bool> MediaRequests::MaybeRemoveRequest(
    const std::string& app_id,
    const content::WebContents* web_contents,
    AppIdToWebContents& app_id_to_web_contents) {
  auto it = app_id_to_web_contents.find(app_id);
  if (it == app_id_to_web_contents.end() ||
      it->second.find(web_contents) == it->second.end()) {
    return absl::nullopt;
  }

  it->second.erase(web_contents);
  if (it->second.empty()) {
    app_id_to_web_contents.erase(it);
    return false;
  }

  return absl::nullopt;
}

absl::optional<bool> MediaRequests::MaybeRemoveRequest(
    const std::string& app_id,
    AppIdToWebContents& app_id_to_web_contents) {
  auto it = app_id_to_web_contents.find(app_id);
  if (it == app_id_to_web_contents.end()) {
    return absl::nullopt;
  }

  app_id_to_web_contents.erase(it);
  return false;
}

}  // namespace apps
