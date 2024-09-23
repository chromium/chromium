// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/parsed_dial_app_info.h"

#include "base/notreached.h"

namespace media_router {

std::string DialAppStateToString(DialAppState app_state) {
  switch (app_state) {
    case DialAppState::kUnknown:
      return "unknown";
    case DialAppState::kRunning:
      return "running";
    case DialAppState::kStopped:
      return "stopped";
  }
  NOTREACHED();
}

ParsedDialAppInfo::ParsedDialAppInfo() = default;
ParsedDialAppInfo::ParsedDialAppInfo(const ParsedDialAppInfo& other) = default;
ParsedDialAppInfo::~ParsedDialAppInfo() = default;

bool ParsedDialAppInfo::operator==(const ParsedDialAppInfo& other) const {
  return dial_version == other.dial_version && name == other.name &&
         allow_stop == other.allow_stop && state == other.state &&
         href == other.href && extra_data == other.extra_data;
}

}  // namespace media_router
