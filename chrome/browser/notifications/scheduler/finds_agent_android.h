// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_FINDS_AGENT_ANDROID_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_FINDS_AGENT_ANDROID_H_

#include "base/android/jni_android.h"
#include "chrome/browser/notifications/scheduler/public/finds_agent.h"

class FindsAgentAndroid : public notifications::FindsAgent {
 public:
  FindsAgentAndroid();
  ~FindsAgentAndroid() override;

 private:
  void OpenNotificationUrl(const GURL& url) override;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_FINDS_AGENT_ANDROID_H_
