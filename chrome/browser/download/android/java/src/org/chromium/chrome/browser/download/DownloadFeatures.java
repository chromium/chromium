// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import org.chromium.base.DeviceInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Utility class for download code interacting with features and params. */
@NullMarked
public final class DownloadFeatures {
    /** Private constructor to avoid instantiation. */
    private DownloadFeatures() {}

    /**
     * Returns whether the download toolbar button is enabled. The button is only available on
     * desktop (Android XR / large-screen) form factors behind {@link
     * ChromeFeatureList#DOWNLOAD_TOOLBAR_BUTTON_FOR_DESKTOP}.
     */
    public static boolean isDownloadToolbarButtonEnabled() {
        return ChromeFeatureList.sDownloadToolbarButtonForDesktop.isEnabled()
                && DeviceInfo.isDesktop();
    }
}
