// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.launchpad;

import android.graphics.Bitmap;

import org.chromium.chrome.browser.browserservices.intents.WebApkExtras.ShortcutItem;

import java.util.List;

/**
 * A plain old data class that holds webapk info for each launchpad item.
 */
class LaunchpadItem {
    public final String packageName;
    public final String shortName;
    public final String name;
    public final String url;
    public final Bitmap icon;
    public final List<ShortcutItem> shortcutItems;

    LaunchpadItem(String packageName, String shortName, String name, String url, Bitmap icon,
            List<ShortcutItem> shortcutItems) {
        this.packageName = packageName;
        this.shortName = shortName;
        this.name = name;
        this.url = url;
        this.icon = icon;
        this.shortcutItems = shortcutItems;
    }
}
