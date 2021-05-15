// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app;

import android.app.Activity;

import org.chromium.chrome.browser.download.DownloadActivity;
import org.chromium.chrome.browser.download.DownloadController;
import org.chromium.ui.base.AndroidPermissionDelegate;

/** Handles changes to notifications based on user action or timeout. */
public class ChromeAndroidPermissionDelegateSupplier
        implements DownloadController.AndroidPermissionDelegateSupplier {
    @Override
    public AndroidPermissionDelegate getDelegate(Activity activity) {
        if (activity instanceof ChromeActivity) {
            return ((ChromeActivity) activity).getWindowAndroid();
        } else if (activity instanceof DownloadActivity) {
            return ((DownloadActivity) activity).getAndroidPermissionDelegate();
        }
        return null;
    }
}
