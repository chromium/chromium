// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_PARSED_DIAL_APP_INFO_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_PARSED_DIAL_APP_INFO_H_

#include <map>
#include <string>

namespace media_router {

// Possible states of a DIAL application.
enum class DialAppState {
  kUnknown = 0,

  // The application is installed and either starting or running.
  kRunning,

  // The application is installed and not running.
  kStopped
};

std::string DialAppStateToString(DialAppState app_state);

struct ParsedDialAppInfo {
  ParsedDialAppInfo();
  ParsedDialAppInfo(const ParsedDialAppInfo& other);
  ~ParsedDialAppInfo();

  bool operator==(const ParsedDialAppInfo& other) const;

  // Identifies the DIAL protocol version associated with the response.
  std::string dial_version;

  // The application name. Mandatory.
  std::string name;

  // Whether the DELETE operation is supported to stop a running application.
  bool allow_stop = true;

  // The reported state of the application.
  DialAppState state = DialAppState::kUnknown;

  // If the applications's state is RUNNING, the resource name for the running
  // application.
  std::string href;

  // Application-specific data included with the GET response that is not part
  // of the official specifications. Map of additional data value keyed by XML
  // tag name.
  std::map<std::string, std::string> extra_data;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_PARSED_DIAL_APP_INFO_H_
