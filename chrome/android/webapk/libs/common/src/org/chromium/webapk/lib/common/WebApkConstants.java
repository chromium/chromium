// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.lib.common;

/**
 * Stores WebAPK related constants.
 */
public final class WebApkConstants {
    public static final String WEBAPK_PACKAGE_PREFIX = "org.chromium.webapk";

    // WebAPK id prefix. The id is used for storing WebAPK data in Chrome's SharedPreferences.
    public static final String WEBAPK_ID_PREFIX = "webapk-";

    /** These EXTRA_* values must stay in sync with
     * {@link org.chromium.chrome.browser.ShortcutHelper}.
     */
    public static final String EXTRA_URL = "org.chromium.chrome.browser.webapp_url";
    public static final String EXTRA_SOURCE = "org.chromium.chrome.browser.webapp_source";
    public static final String EXTRA_WEBAPK_PACKAGE_NAME =
            "org.chromium.chrome.browser.webapk_package_name";
    public static final String EXTRA_WEBAPK_LAUNCHING_ACTIVITY_CLASS_NAME =
            "org.chromium.webapk.launching_activity_class_name";
    public static final String EXTRA_FORCE_NAVIGATION =
            "org.chromium.chrome.browser.webapk_force_navigation";
    // Activity launch time for uma tracking of Chrome web apk startup
    public static final String EXTRA_WEBAPK_LAUNCH_TIME =
            "org.chromium.chrome.browser.webapk_launch_time";
    // Whether the host browser's activity should be completely transparent till the page
    // has loaded. This enables the ShellAPK's splash screen to show through the host browser's
    // activity.
    public static final String EXTRA_USE_TRANSPARENT_SPLASH =
            "org.chromium.chrome.browser.webapk.transparent_splash";

    // Must be kept in sync with chrome/browser/android/shortcut_info.h.
    public static final int SHORTCUT_SOURCE_UNKNOWN = 0;
    public static final int SHORTCUT_SOURCE_EXTERNAL_INTENT = 9;
    public static final int SHORTCUT_SOURCE_SHARE = 13;

    /** Name of the shared preferences file. */
    public static final String PREF_PACKAGE = "org.chromium.webapk.shell_apk";
}
