// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_PROTOCOL_SYSTEM_INFO_HANDLER_H_
#define CHROME_BROWSER_DEVTOOLS_PROTOCOL_SYSTEM_INFO_HANDLER_H_

#include "chrome/browser/devtools/protocol/system_info.h"

namespace content {
class protocol;
}  // namespace content

class SystemInfoHandler : public protocol::SystemInfo::Backend {
 public:
  explicit SystemInfoHandler(protocol::UberDispatcher* dispatcher);

  SystemInfoHandler(const SystemInfoHandler&) = delete;
  SystemInfoHandler& operator=(const SystemInfoHandler&) = delete;

  ~SystemInfoHandler() override;

 private:
  protocol::Response GetFeatureState(const std::string& in_featureState,
                                     bool* featureEnabled) override;
};

#endif  // CHROME_BROWSER_DEVTOOLS_PROTOCOL_SYSTEM_INFO_HANDLER_H_
