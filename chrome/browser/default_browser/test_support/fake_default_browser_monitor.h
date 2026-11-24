// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEFAULT_BROWSER_TEST_SUPPORT_FAKE_DEFAULT_BROWSER_MONITOR_H_
#define CHROME_BROWSER_DEFAULT_BROWSER_TEST_SUPPORT_FAKE_DEFAULT_BROWSER_MONITOR_H_

#include "chrome/browser/default_browser/default_browser_monitor.h"

namespace default_browser {

// Doesn't watch the system. Tests call `TriggerChange()` instead.
class FakeDefaultBrowserMonitor : public DefaultBrowserMonitor {
 public:
  FakeDefaultBrowserMonitor();
  ~FakeDefaultBrowserMonitor() override;

  void StartMonitor() override;

  void TriggerChange();
};

}  // namespace default_browser

#endif  // CHROME_BROWSER_DEFAULT_BROWSER_TEST_SUPPORT_FAKE_DEFAULT_BROWSER_MONITOR_H_
