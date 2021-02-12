// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/media_requests.h"

#include <utility>

#include "base/optional.h"

namespace apps {

AccessingRequest::AccessingRequest(base::Optional<bool> camera,
                                   base::Optional<bool> microphone)
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

  base::Optional<bool> accessing_camera;
  base::Optional<bool> accessing_microphone;
  if (state == content::MEDIA_REQUEST_STATE_DONE) {
    if (blink::IsVideoInputMediaType(stream_type)) {
      accessing_camera = MaybeAddRequest(app_id, web_contents,
                                         app_id_to_web_contents_for_camera_);
    }
    if (blink::IsAudioInputMediaType(stream_type)) {
      accessing_microphone = MaybeAddRequest(
          app_id, web_contents, app_id_to_web_contents_for_microphone_);
    }
  }

  if (state == content::MEDIA_REQUEST_STATE_CLOSING) {
    if (blink::IsVideoInputMediaType(stream_type)) {
      accessing_camera = MaybeRemoveRequest(app_id, web_contents,
                                            app_id_to_web_contents_for_camera_);
    }
    if (blink::IsAudioInputMediaType(stream_type)) {
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
    const std::map<std::string, std::set<const content::WebContents*>>&
        app_id_to_web_contents) {
  auto it = app_id_to_web_contents.find(app_id);
  if (it != app_id_to_web_contents.end() &&
      it->second.find(web_contents) != it->second.end()) {
    return true;
  }
  return false;
}

base::Optional<bool> MediaRequests::MaybeAddRequest(
    const std::string& app_id,
    const content::WebContents* web_contents,
    std::map<std::string, std::set<const content::WebContents*>>&
        app_id_to_web_contents) {
  auto it = app_id_to_web_contents.find(app_id);
  if (it != app_id_to_web_contents.end() &&
      it->second.find(web_contents) != it->second.end()) {
    return base::nullopt;
  }

  base::Optional<bool> ret;
  if (it == app_id_to_web_contents.end()) {
    ret = true;
    app_id_to_web_contents[app_id].insert(web_contents);
  } else {
    it->second.insert(web_contents);
  }

  return ret;
}

base::Optional<bool> MediaRequests::MaybeRemoveRequest(
    const std::string& app_id,
    const content::WebContents* web_contents,
    std::map<std::string, std::set<const content::WebContents*>>&
        app_id_to_web_contents) {
  auto it = app_id_to_web_contents.find(app_id);
  if (it == app_id_to_web_contents.end() ||
      it->second.find(web_contents) == it->second.end()) {
    return base::nullopt;
  }

  it->second.erase(web_contents);
  if (it->second.empty()) {
    app_id_to_web_contents.erase(it);
    return false;
  }

  return base::nullopt;
}

base::Optional<bool> MediaRequests::MaybeRemoveRequest(
    const std::string& app_id,
    std::map<std::string, std::set<const content::WebContents*>>&
        app_id_to_web_contents) {
  auto it = app_id_to_web_contents.find(app_id);
  if (it == app_id_to_web_contents.end()) {
    return base::nullopt;
  }

  app_id_to_web_contents.erase(it);
  return false;
}

}  // namespace apps
