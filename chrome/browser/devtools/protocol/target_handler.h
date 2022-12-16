// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_PROTOCOL_TARGET_HANDLER_H_
#define CHROME_BROWSER_DEVTOOLS_PROTOCOL_TARGET_HANDLER_H_

#include <set>

#include "chrome/browser/devtools/protocol/target.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "net/base/host_port_pair.h"

using RemoteLocations = std::set<net::HostPortPair>;

class TargetHandler : public protocol::Target::Backend {
 public:
  TargetHandler(protocol::UberDispatcher* dispatcher, bool is_trusted);

  TargetHandler(const TargetHandler&) = delete;
  TargetHandler& operator=(const TargetHandler&) = delete;

  ~TargetHandler() override;

  RemoteLocations& remote_locations() { return remote_locations_; }

  // Target::Backend:
  protocol::Response SetRemoteLocations(
      std::unique_ptr<protocol::Array<protocol::Target::RemoteLocation>>
          in_locations) override;
  protocol::Response CreateTarget(
      const std::string& url,
      protocol::Maybe<int> width,
      protocol::Maybe<int> height,
      protocol::Maybe<std::string> browser_context_id,
      protocol::Maybe<bool> enable_begin_frame_control,
      protocol::Maybe<bool> new_window,
      protocol::Maybe<bool> background,
      protocol::Maybe<bool> for_tab,
      std::string* out_target_id) override;

 private:
  RemoteLocations remote_locations_;
  const bool is_trusted_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_PROTOCOL_TARGET_HANDLER_H_
