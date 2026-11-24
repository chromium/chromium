// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_PROTOCOL_TARGET_HANDLER_ANDROID_H_
#define CHROME_BROWSER_DEVTOOLS_PROTOCOL_TARGET_HANDLER_ANDROID_H_

#include <set>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/devtools/protocol/target.h"
#include "net/base/host_port_pair.h"

namespace content {
class DevToolsAgentHostClientChannel;
}  // namespace content

using RemoteLocations = std::set<net::HostPortPair>;

class BrowserHandlerAndroid;

class TargetHandlerAndroid : public protocol::Target::Backend {
 public:
  TargetHandlerAndroid(protocol::UberDispatcher* dispatcher,
                       bool is_trusted,
                       bool may_read_local_files,
                       BrowserHandlerAndroid* browser_handler,
                       content::DevToolsAgentHostClientChannel* channel);

  TargetHandlerAndroid(const TargetHandlerAndroid&) = delete;
  TargetHandlerAndroid& operator=(const TargetHandlerAndroid&) = delete;

  ~TargetHandlerAndroid() override;

  RemoteLocations& remote_locations() { return remote_locations_; }

  // Target::Backend:
  protocol::Response SetRemoteLocations(
      std::unique_ptr<protocol::Array<protocol::Target::RemoteLocation>>
          in_locations) override;
  // Creates a new target. For Browser-level sessions, creates the target in
  // `browser_context_id` if specified or the default profile otherwise. For
  // non-Browser sessions, rejects `browser_context_id` and creates the target
  // in the attached target's own BrowserContext.
  protocol::Response CreateTarget(
      const std::string& url,
      std::optional<int> left,
      std::optional<int> top,
      std::optional<int> width,
      std::optional<int> height,
      std::optional<std::string> window_state,
      std::optional<std::string> browser_context_id,
      std::optional<bool> enable_begin_frame_control,
      std::optional<bool> new_window,
      std::optional<bool> background,
      std::optional<bool> for_tab,
      std::optional<bool> hidden,
      std::optional<bool> focus,
      std::string* out_target_id) override;

 private:
  bool is_trusted_ = false;
  bool may_read_local_files_ = false;
  raw_ptr<BrowserHandlerAndroid> browser_handler_;
  // Owned by the enclosing `content::DevToolsSession`, which outlives `this`.
  const raw_ptr<content::DevToolsAgentHostClientChannel> channel_;
  RemoteLocations remote_locations_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_PROTOCOL_TARGET_HANDLER_ANDROID_H_
