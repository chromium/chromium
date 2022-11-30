// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.video_tutorials;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.AppHooks;

/**
 * Class for providing helper method for talking with the video
 * tutorial service.
 */
public class VideoTutorialsServiceUtils {
    /**
     * @return Default server URL for getting video tutorials.
     */
    @CalledByNative
    private static String getDefaultServerUrl() {
        // TODO(qinmin): having a separate method in AppHooks to provide
        // server URLs for video tutorials.
        return AppHooks.get().getDefaultQueryTilesServerUrl();
    }
}
