// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/public/tips_agent.h"

namespace notifications {

// Default implementation of TipsAgent.
class TipsAgentDefault : public TipsAgent {
 public:
  TipsAgentDefault() = default;
  TipsAgentDefault(const TipsAgentDefault&) = delete;
  TipsAgentDefault& operator=(const TipsAgentDefault&) = delete;
  ~TipsAgentDefault() override = default;

 private:
  void ShowTipsPromo(TipsNotificationsFeatureType feature_type) override {}
};

// static
std::unique_ptr<TipsAgent> TipsAgent::Create() {
  return std::make_unique<TipsAgentDefault>();
}

}  // namespace notifications
