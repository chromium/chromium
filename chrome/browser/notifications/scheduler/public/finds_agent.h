// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_FINDS_AGENT_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_FINDS_AGENT_H_

#include <memory>
#include <string>

#include "url/gurl.h"

namespace notifications {

// Does the work to act on interactions with finds notifications.
class FindsAgent {
 public:
  // Creates the default FindsAgent.
  static std::unique_ptr<FindsAgent> Create();

  // Opens the specified URL in a new tab using an intent.
  virtual void OpenNotificationUrl(const GURL& url) = 0;

  FindsAgent(const FindsAgent&) = delete;
  FindsAgent& operator=(const FindsAgent&) = delete;
  virtual ~FindsAgent() = default;

 protected:
  FindsAgent() = default;
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_FINDS_AGENT_H_
