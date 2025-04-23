// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.screenshot_monitor;

import org.chromium.build.annotations.NullMarked;

/** This class serves as a callback from ScreenshotMonitor. */
@NullMarked
public interface ScreenshotMonitorDelegate {
    void onScreenshotTaken();
}
