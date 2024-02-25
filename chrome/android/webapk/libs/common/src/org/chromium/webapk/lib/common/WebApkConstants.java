// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.lib.common;

/** Stores WebAPK related constants. */
public final class WebApkConstants {
    // WebAPK id prefix. The id is used for storing WebAPK data in Chrome's SharedPreferences.
    public static final String WEBAPK_ID_PREFIX = "webapk-";

    // These EXTRA_* values must stay in sync with {@link
    // org.chromium.chrome.browser.ShortcutHelper}.
    public static final String EXTRA_URL = "org.chromium.chrome.browser.webapp_url";
    public static final String EXTRA_SOURCE = "org.chromium.chrome.browser.webapp_source";
    public static final String EXTRA_WEBAPK_PACKAGE_NAME =
            "org.chromium.chrome.browser.webapk_package_name";
    public static final String EXTRA_WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME =
            "org.chromium.webapk.selected_share_target_activity_class_name";
    public static final String EXTRA_FORCE_NAVIGATION =
            "org.chromium.chrome.browser.webapk_force_navigation";
    // Activity launch time for uma tracking of Chrome web apk startup
    public static final String EXTRA_WEBAPK_LAUNCH_TIME =
            "org.chromium.chrome.browser.webapk_launch_time";
    public static final String EXTRA_NEW_STYLE_SPLASH_SHOWN_TIME =
            "org.chromium.webapk.new_style_splash_shown_time";
    // Whether the WebAPK provides a splash screen activity which should be launched by the host
    // browser to hide the web contents while the page is loading.
    public static final String EXTRA_SPLASH_PROVIDED_BY_WEBAPK =
            "org.chromium.chrome.browser.webapk.splash_provided_by_webapk";
    // Tells the host browser to relaunch the WebAPK.
    public static final String EXTRA_RELAUNCH = "org.chromium.webapk.relaunch";
    public static final String EXTRA_IS_WEBAPK = "org.chromium.webapk.is_webapk";

    // Must be kept in sync with components/webapps/browser/android/shortcut_info.h.
    public @interface ShortcutSource {
        int UNKNOWN = 0;
        int EXTERNAL_INTENT = 9;
        int WEBAPK_SHARE_TARGET = 13;
    }

    /** Name of the shared preferences file. */
    public static final String PREF_PACKAGE = "org.chromium.webapk.shell_apk";
}
