// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.metrics;

/** A class to manage communication with AppUpdate API. */
public class AppUpdateInfo {
    private final AppUpdateInfoDelegate mDelegate;
    private static final AppUpdateInfo sInstance = new AppUpdateInfo();

    public static AppUpdateInfo getInstance() {
        return sInstance;
    }

    public AppUpdateInfo() {
        mDelegate = new AppUpdateInfoDelegateImpl();
    }

    public void emitToHistogram() {
        mDelegate.emitToHistogram();
    }
}
