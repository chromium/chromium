// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.launchpad;

import android.graphics.Bitmap;

/**
 * A plain old data class that holds webapk info for each launchpad item.
 */
class LaunchpadItem {
    public final String packageName;
    public final String shortName;
    public final String name;
    public final String url;
    public final Bitmap icon;

    LaunchpadItem(String packageName, String shortName, String name, String url, Bitmap icon) {
        this.packageName = packageName;
        this.shortName = shortName;
        this.name = name;
        this.url = url;
        this.icon = icon;
    }
}
