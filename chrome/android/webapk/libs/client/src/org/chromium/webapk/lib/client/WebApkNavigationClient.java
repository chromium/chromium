// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.lib.client;

import android.content.Intent;
import android.net.Uri;

import org.chromium.webapk.lib.common.WebApkConstants;

/** WebApkNavigationClient provides an API to get an intent to launch a WebAPK. */
public class WebApkNavigationClient {
    /**
     * Creates intent to launch a WebAPK.
     *
     * @param webApkPackageName Package name of the WebAPK to launch.
     * @param url URL to navigate WebAPK to.
     * @param forceNavigation Whether the WebAPK should be navigated to the url if the WebAPK is
     *     already open. If {@link forceNavigation} is false and the WebAPK is already running, the
     *     WebAPK will be brought to the foreground.
     * @return The intent.
     */
    public static Intent createLaunchWebApkIntent(
            String webApkPackageName, String url, boolean forceNavigation) {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        intent.setPackage(webApkPackageName);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(WebApkConstants.EXTRA_FORCE_NAVIGATION, forceNavigation);
        return intent;
    }
}
