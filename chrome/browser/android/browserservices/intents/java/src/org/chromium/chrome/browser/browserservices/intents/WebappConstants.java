// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.intents;

import org.chromium.content_public.common.ScreenOrientationConstants;

/**
 * This class contains constants related to adding shortcuts to the Android Home
 * screen.  These shortcuts are used to either open a page in the main browser
 * or open a web app.
 */
public class WebappConstants {
    public static final String EXTRA_ICON = "org.chromium.chrome.browser.webapp_icon";
    public static final String EXTRA_ID = "org.chromium.chrome.browser.webapp_id";
    public static final String EXTRA_MAC = "org.chromium.chrome.browser.webapp_mac";
    // EXTRA_TITLE is present for backward compatibility reasons.
    public static final String EXTRA_TITLE = "org.chromium.chrome.browser.webapp_title";
    public static final String EXTRA_NAME = "org.chromium.chrome.browser.webapp_name";
    public static final String EXTRA_SHORT_NAME = "org.chromium.chrome.browser.webapp_short_name";
    public static final String EXTRA_URL = "org.chromium.chrome.browser.webapp_url";
    public static final String EXTRA_SCOPE = "org.chromium.chrome.browser.webapp_scope";
    public static final String EXTRA_DISPLAY_MODE =
            "org.chromium.chrome.browser.webapp_display_mode";
    public static final String EXTRA_ORIENTATION = ScreenOrientationConstants.EXTRA_ORIENTATION;
    public static final String EXTRA_SOURCE = "org.chromium.chrome.browser.webapp_source";
    public static final String EXTRA_THEME_COLOR = "org.chromium.chrome.browser.theme_color";
    public static final String EXTRA_BACKGROUND_COLOR =
            "org.chromium.chrome.browser.background_color";
    public static final String EXTRA_DARK_THEME_COLOR =
            "org.chromium.chrome.browser.dark_theme_color";
    public static final String EXTRA_DARK_BACKGROUND_COLOR =
            "org.chromium.chrome.browser.dark_background_color";
    public static final String EXTRA_IS_ICON_GENERATED =
            "org.chromium.chrome.browser.is_icon_generated";
    public static final String EXTRA_IS_ICON_ADAPTIVE =
            "org.chromium.chrome.browser.webapp_icon_adaptive";
    public static final String EXTRA_VERSION =
            "org.chromium.chrome.browser.webapp_shortcut_version";
    public static final String REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB =
            "REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB";
    // Whether the webapp should navigate to the URL in {@link EXTRA_URL} if the webapp is already
    // open. Applies to webapps and WebAPKs. Value contains "webapk" for backward compatibility.
    public static final String EXTRA_FORCE_NAVIGATION =
            "org.chromium.chrome.browser.webapk_force_navigation";

    // When a new field is added to the intent, this version should be incremented so that it will
    // be correctly populated into the WebappRegistry/WebappDataStorage.
    public static final int WEBAPP_SHORTCUT_VERSION = 3;

    private WebappConstants() {}
}
