// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/public/finds_agent.h"

namespace notifications {

// Default implementation of FindsAgent.
class FindsAgentDefault : public FindsAgent {
 public:
  FindsAgentDefault() = default;
  FindsAgentDefault(const FindsAgentDefault&) = delete;
  FindsAgentDefault& operator=(const FindsAgentDefault&) = delete;
  ~FindsAgentDefault() override = default;

 private:
  void OpenNotificationUrl(const GURL& url) override {}
};

// static
std::unique_ptr<FindsAgent> FindsAgent::Create() {
  return std::make_unique<FindsAgentDefault>();
}

}  // namespace notifications
