// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/aw_media_drm_bridge_client.h"

#include <utility>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace android_webview {

namespace {

const size_t kGUIDLength = 36U;

#define RCHECK(x)                                    \
  if (!(x)) {                                        \
    LOG(ERROR) << "Can't parse key-system mapping: " \
               << key_system_uuid_mapping;           \
    return std::make_pair("", uuid);                 \
  }

media::MediaDrmBridgeClient::KeySystemUuidMap::value_type
CreateMappingFromString(const std::string& key_system_uuid_mapping) {
  std::vector<uint8_t> uuid;

  std::vector<std::string> tokens =
      base::SplitString(key_system_uuid_mapping, ",", base::KEEP_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  RCHECK(tokens.size() == 2);

  std::string key_system;
  base::TrimWhitespaceASCII(tokens[0], base::TRIM_ALL, &key_system);

  std::string guid(tokens[1]);
  RCHECK(guid.length() == kGUIDLength);
  base::RemoveChars(guid, "-", &guid);
  RCHECK(base::HexStringToBytes(guid, &uuid));

  return std::make_pair(key_system, uuid);
}

}  // namespace

AwMediaDrmBridgeClient::AwMediaDrmBridgeClient(
    const std::vector<std::string>& key_system_uuid_mappings)
    : key_system_uuid_mappings_(key_system_uuid_mappings) {}

AwMediaDrmBridgeClient::~AwMediaDrmBridgeClient() {}

void AwMediaDrmBridgeClient::AddKeySystemUUIDMappings(KeySystemUuidMap* map) {
  for (const std::string& key_system_uuid_mapping : key_system_uuid_mappings_) {
    auto mapping = CreateMappingFromString(key_system_uuid_mapping);
    if (!mapping.first.empty())
      map->insert(mapping);
  }
}

media::MediaDrmBridgeDelegate*
AwMediaDrmBridgeClient::GetMediaDrmBridgeDelegate(
    const media::UUID& scheme_uuid) {
  if (scheme_uuid == widevine_delegate_.GetUUID())
    return &widevine_delegate_;
  if (scheme_uuid == clearkey_delegate_.GetUUID()) {
    return &clearkey_delegate_;
  }
  return nullptr;
}

}  // namespace android_webview
