// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/controlled_frame/controlled_frame_user_agent_util.h"

#include <optional>
#include <string>

#include "base/version_info/version_info.h"
#include "components/embedder_support/user_agent_utils.h"
#include "third_party/blink/public/common/user_agent/user_agent_brand_version_type.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"

namespace {

constexpr char kControlledFrameBrand[] = "ControlledFrame";

std::string GetChromeDefaultUserAgent() {
  return embedder_support::GetUserAgent();
}

blink::UserAgentMetadata GetControlledFrameUserAgentMetadata() {
  auto metadata = embedder_support::GetUserAgentMetadata();

  metadata.brand_version_list =
      embedder_support::GetUserAgentBrandMajorVersionList(
          blink::UserAgentBrandVersion(kControlledFrameBrand,
                                       version_info::GetMajorVersionNumber()));

  metadata.brand_full_version_list =
      embedder_support::GetUserAgentBrandFullVersionList(
          blink::UserAgentBrandVersion(kControlledFrameBrand,
                                       metadata.full_version));

  return metadata;
}

}  // namespace

namespace controlled_frame {

blink::UserAgentOverride GetDefaultControlledFrameUserAgentOverride() {
  blink::UserAgentOverride result;
  result.ua_string_override = GetChromeDefaultUserAgent();
  result.ua_metadata_override = GetControlledFrameUserAgentMetadata();
  return result;
}

}  // namespace controlled_frame
