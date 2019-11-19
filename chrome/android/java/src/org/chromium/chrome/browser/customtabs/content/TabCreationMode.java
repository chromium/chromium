// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.init.StartupTabPreloader;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Specifies the way the initial Tab in a Custom Tab activity was created.
 */
@IntDef({TabCreationMode.NONE, TabCreationMode.DEFAULT, TabCreationMode.EARLY,
        TabCreationMode.RESTORED, TabCreationMode.HIDDEN,
        TabCreationMode.FROM_STARTUP_TAB_PRELOADER})
@Retention(RetentionPolicy.SOURCE)
public @interface TabCreationMode {
    /** The tab has not been created yet */
    int NONE = 0;

    /** New tab that was created on native initialization. */
    int DEFAULT = 1;

    /** A tab that was restored after activity re-creation. */
    int RESTORED = 2;

    /**
     * A new tab that was created in onPreInflationStartup, after
     * {@link CustomTabsConnection#warmup)} has finished.
     */
    int EARLY = 3;

    /**
     * A hidden tab that was created preemptively via {@link CustomTabsConnection#mayLaunchUrl}.
     */
    int HIDDEN = 4;

    /** Opened speculatively by the {@link StartupTabPreloader}.. */
    int FROM_STARTUP_TAB_PRELOADER = 5;
}
