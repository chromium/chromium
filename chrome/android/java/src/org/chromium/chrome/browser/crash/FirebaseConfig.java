// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crash;

import org.chromium.base.BuildConfig;
import org.chromium.components.version_info.Channel;
import org.chromium.components.version_info.VersionConstants;

/**
 * Simple class containing build specific Firebase app IDs.
 */
public class FirebaseConfig {
    private FirebaseConfig() {}

    /**
     * Get the Firebase app ID that should be uploaded with crashes to enable deobfuscation.
     * See http://goto.google.com/clank/engineering/sdk-build/proguard for more info.
     *
     * @return the channel dependent Firebase app ID.
     */
    public static String getFirebaseAppId() {
        if (BuildConfig.IS_CHROME_BRANDED) {
            if (VersionConstants.CHANNEL == Channel.CANARY) {
                return "1:850546144789:android:54d7b17bce961ff1";
            } else if (VersionConstants.CHANNEL == Channel.DEV) {
                return "1:573639067789:android:51b6bb8c28a80880";
            } else if (VersionConstants.CHANNEL == Channel.BETA) {
                return "1:555018597840:android:685a0b5814643d3e";
            } else if (VersionConstants.CHANNEL == Channel.STABLE) {
                return "1:914760932289:android:bdb905fe8b8890ae";
            }
        }
        return "";
    }
}