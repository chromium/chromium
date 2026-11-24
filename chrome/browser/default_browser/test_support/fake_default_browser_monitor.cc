// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/test_support/fake_default_browser_monitor.h"

namespace default_browser {

FakeDefaultBrowserMonitor::FakeDefaultBrowserMonitor() = default;

FakeDefaultBrowserMonitor::~FakeDefaultBrowserMonitor() = default;

void FakeDefaultBrowserMonitor::StartMonitor() {}

void FakeDefaultBrowserMonitor::TriggerChange() {
  NotifyObservers();
}

}  // namespace default_browser
