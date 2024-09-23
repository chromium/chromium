// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/webrtc_request_builder.h"

#include "base/check.h"
#include "base/i18n/timezone.h"
#include "base/unguessable_token.h"
#include "chrome/browser/nearby_sharing/instantmessaging/proto/instantmessaging.pb.h"

namespace {

const char kAppName[] = "Nearby";
constexpr int kMajorVersion = 1;
constexpr int kMinorVersion = 24;
constexpr int kPointVersion = 0;

using LocationStandard_Format =
    chrome_browser_nearby_sharing_instantmessaging::LocationStandard_Format;

LocationStandard_Format ToProto(
    ::sharing::mojom::LocationStandardFormat format) {
  switch (format) {
    case ::sharing::mojom::LocationStandardFormat::E164_CALLING:
      return LocationStandard_Format::LocationStandard_Format_E164_CALLING;
    case ::sharing::mojom::LocationStandardFormat::ISO_3166_1_ALPHA_2:
      return LocationStandard_Format::
          LocationStandard_Format_ISO_3166_1_ALPHA_2;
  }
}

void BuildLocationHint(
    chrome_browser_nearby_sharing_instantmessaging::LocationHint* location_hint,
    ::sharing::mojom::LocationHintPtr location_hint_ptr) {
  location_hint->set_location(location_hint_ptr->location);
  location_hint->set_format(ToProto(location_hint_ptr->format));
}

void BuildId(chrome_browser_nearby_sharing_instantmessaging::Id* req_id,
             const std::string& id,
             ::sharing::mojom::LocationHintPtr location_hint) {
  DCHECK(req_id);
  req_id->set_id(id);
  req_id->set_app(kAppName);
  req_id->set_type(
      chrome_browser_nearby_sharing_instantmessaging::IdType::NEARBY_ID);
  BuildLocationHint(req_id->mutable_location_hint(), std::move(location_hint));
}

void BuildHeader(
    chrome_browser_nearby_sharing_instantmessaging::RequestHeader* header,
    const std::string& requester_id,
    ::sharing::mojom::LocationHintPtr location_hint) {
  DCHECK(header);
  header->set_request_id(base::UnguessableToken::Create().ToString());
  header->set_app(kAppName);
  BuildId(header->mutable_requester_id(), requester_id,
          std::move(location_hint));
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
BuildSendRequest(const std::string& self_id,
                 const std::string& peer_id,
                 ::sharing::mojom::LocationHintPtr location_hint) {
  chrome_browser_nearby_sharing_instantmessaging::SendMessageExpressRequest
      request;
  BuildId(request.mutable_dest_id(), peer_id, location_hint->Clone());
  BuildHeader(request.mutable_header(), self_id, location_hint->Clone());
  return request;
}

chrome_browser_nearby_sharing_instantmessaging::ReceiveMessagesExpressRequest
BuildReceiveRequest(const std::string& self_id,
                    ::sharing::mojom::LocationHintPtr location_hint) {
  chrome_browser_nearby_sharing_instantmessaging::ReceiveMessagesExpressRequest
      request;
  BuildHeader(request.mutable_header(), self_id, std::move(location_hint));
  return request;
}
