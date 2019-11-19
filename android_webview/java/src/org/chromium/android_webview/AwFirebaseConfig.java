// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.base.BuildConfig;

/**
 * Simple class containing build specific Firebase app IDs.
 */
public class AwFirebaseConfig {
    private AwFirebaseConfig() {}

    /**
     * Get the Firebase app ID that should be uploaded with crashes to enable deobfuscation.
     * See http://goto.google.com/clank/engineering/sdk-build/proguard for more info.
     *
     * @return the Firebase app ID.
     */
    public static String getFirebaseAppId() {
        // TODO(crbug.com/958454): Support per-channel Firebase app IDs for Webview.
        if (BuildConfig.IS_CHROME_BRANDED) {
            return "1:885372672379:android:e1ff119a3219cbe0";
        }
        return "";
    }
}