// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_TIPS_AGENT_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_TIPS_AGENT_H_

#include <memory>

#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"

namespace notifications {

// Does the work to schedule and act on tips notifications in Android UI.
class TipsAgent {
 public:
  // Creates the default TipsAgent.
  static std::unique_ptr<TipsAgent> Create();

  // Shows the tips promo in UI.
  virtual void ShowTipsPromo(TipsNotificationsFeatureType feature_type) = 0;

  TipsAgent(const TipsAgent&) = delete;
  TipsAgent& operator=(const TipsAgent&) = delete;
  virtual ~TipsAgent() = default;

 protected:
  TipsAgent() = default;
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_TIPS_AGENT_H_
