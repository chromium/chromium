// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/media_requests.h"

#include <optional>
#include <utility>

namespace apps {

AccessingRequest::AccessingRequest(std::optional<bool> camera,
                                   std::optional<bool> microphone)
    : camera(camera), microphone(microphone) {}

AccessingRequest::AccessingRequest(AccessingRequest&&) = default;

AccessingRequest& AccessingRequest::operator=(AccessingRequest&&) = default;

AccessingRequest::~AccessingRequest() = default;

MediaRequests::MediaRequests() = default;

MediaRequests::~MediaRequests() = default;

AccessingRequest MediaRequests::UpdateMicrophoneState(
    const std::string& app_id,
    const content::WebContents* web_contents,
    bool is_accessing_microphone) {
  std::optional<bool> accessing_microphone;
  if (is_accessing_microphone) {
    accessing_microphone = MaybeAddRequest(
        app_id, web_contents, app_id_to_web_contents_for_microphone_);
  } else {
    accessing_microphone = MaybeRemoveRequest(
        app_id, web_contents, app_id_to_web_contents_for_microphone_);
  }
  return AccessingRequest(/*camera=*/std::nullopt, accessing_microphone);
}

AccessingRequest MediaRequests::UpdateCameraState(
    const std::string& app_id,
    const content::WebContents* web_contents,
    bool is_accessing_camera) {
  std::optional<bool> accessing_camera;
  if (is_accessing_camera) {
    accessing_camera = MaybeAddRequest(app_id, web_contents,
                                       app_id_to_web_contents_for_camera_);
  } else {
    accessing_camera = MaybeRemoveRequest(app_id, web_contents,
                                          app_id_to_web_contents_for_camera_);
  }
  return AccessingRequest(accessing_camera, /*microphone=*/std::nullopt);
}

AccessingRequest MediaRequests::RemoveRequests(const std::string& app_id) {
  return AccessingRequest(
      MaybeRemoveRequest(app_id, app_id_to_web_contents_for_camera_),
      MaybeRemoveRequest(app_id, app_id_to_web_contents_for_microphone_));
}

std::optional<bool> MediaRequests::MaybeAddRequest(
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
    return std::nullopt;
  }

  // Not a new request for `app_id`. So return nullopt, which means no change
  // for`app_id`.
  return std::nullopt;
}

std::optional<bool> MediaRequests::MaybeRemoveRequest(
    const std::string& app_id,
    const content::WebContents* web_contents,
    AppIdToWebContents& app_id_to_web_contents) {
  auto it = app_id_to_web_contents.find(app_id);
  if (it == app_id_to_web_contents.end() ||
      it->second.find(web_contents) == it->second.end()) {
    return std::nullopt;
  }

  it->second.erase(web_contents);
  if (it->second.empty()) {
    app_id_to_web_contents.erase(it);
    return false;
  }

  return std::nullopt;
}

std::optional<bool> MediaRequests::MaybeRemoveRequest(
    const std::string& app_id,
    AppIdToWebContents& app_id_to_web_contents) {
  auto it = app_id_to_web_contents.find(app_id);
  if (it == app_id_to_web_contents.end()) {
    return std::nullopt;
  }

  app_id_to_web_contents.erase(it);
  return false;
}

}  // namespace apps
