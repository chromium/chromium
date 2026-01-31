// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.open_in_app;

import org.chromium.base.DeviceInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Utility class for Open in App. */
@NullMarked
public class OpenInAppUtils {

    /** Returns whether Open in App is available. */
    public static boolean isOpenInAppAvailable() {
        return ChromeFeatureList.sDesktopAndroidLinkCapturing.isEnabled() && DeviceInfo.isDesktop();
    }

    private OpenInAppUtils() {}
}
