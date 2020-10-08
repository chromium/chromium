// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/webrtc_request_builder.h"

#include "base/check.h"
#include "base/i18n/timezone.h"
#include "chrome/browser/nearby_sharing/instantmessaging/proto/instantmessaging.pb.h"

namespace {

const char kAppName[] = "Nearby";
constexpr int kMajorVersion = 1;
constexpr int kMinorVersion = 24;
constexpr int kPointVersion = 0;

void BuildLocationHint(
    chrome_browser_nearby_sharing_instantmessaging::LocationHint*
        location_hint) {
  location_hint->set_location(base::CountryCodeForCurrentTimezone());
  location_hint->set_format(chrome_browser_nearby_sharing_instantmessaging::
                                LocationStandard_Format_ISO_3166_1_ALPHA_2);
}

void BuildId(chrome_browser_nearby_sharing_instantmessaging::Id* req_id,
             const std::string& id) {
  DCHECK(req_id);
  req_id->set_id(id);
  req_id->set_app(kAppName);
  req_id->set_type(
      chrome_browser_nearby_sharing_instantmessaging::IdType::NEARBY_ID);
  BuildLocationHint(req_id->mutable_location_hint());
}

void BuildHeader(
    chrome_browser_nearby_sharing_instantmessaging::RequestHeader* header,
    const std::string& requester_id) {
  DCHECK(header);
  header->set_app(kAppName);
  BuildId(header->mutable_requester_id(), requester_id);
  chrome_browser_nearby_sharing_instantmessaging::ClientInfo* info =
      header->mutable_client_info();
  info->set_api_version(
      chrome_browser_nearby_sharing_instantmessaging::ApiVersion::V4);
  info->set_platform_type(
      chrome_browser_nearby_sharing_instantmessaging::Platform::DESKTOP);
  info->set_version_major(kMajorVersion);
  info->set_version_minor(kMinorVersion);
  info->set_version_point(kPointVersion);
}

}  // namespace

chrome_browser_nearby_sharing_instantmessaging::SendMessageExpressRequest
BuildSendRequest(const std::string& self_id, const std::string& peer_id) {
  chrome_browser_nearby_sharing_instantmessaging::SendMessageExpressRequest
      request;
  BuildId(request.mutable_dest_id(), peer_id);
  BuildHeader(request.mutable_header(), self_id);
  return request;
}

chrome_browser_nearby_sharing_instantmessaging::ReceiveMessagesExpressRequest
BuildReceiveRequest(const std::string& self_id) {
  chrome_browser_nearby_sharing_instantmessaging::ReceiveMessagesExpressRequest
      request;
  BuildHeader(request.mutable_header(), self_id);
  return request;
}
