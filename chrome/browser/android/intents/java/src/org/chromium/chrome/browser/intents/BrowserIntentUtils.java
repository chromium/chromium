// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.intents;

import android.app.Activity;
import android.content.Intent;
import android.os.SystemClock;

/** Modularized Intent utilities for use by other feature modules and chrome_java. */
public class BrowserIntentUtils {
    /** Key to associate a startup timestamp in the uptimeMillis timebase with an intent. */
    public static final String EXTRA_STARTUP_UPTIME_MS =
            "org.chromium.chrome.browser.startup.uptime";

    /** Key to associate a startup timestamp in the elapsedRealtime timebase with an intent. */
    public static final String EXTRA_STARTUP_REALTIME_MS =
            "org.chromium.chrome.browser.startup.realtime";

    /** Alias for the ChromeLauncherActivity. */
    public static final String CHROME_LAUNCHER_ACTIVITY_CLASS_NAME =
            "com.google.android.apps.chrome.IntentDispatcher";

    /**
     * Adds two timestamps to an intent, as returned by {@link SystemClock#elapsedRealtime()}.
     *
     * To track page load time, this needs to be called as close as possible to
     * the entry point (in {@link Activity#onCreate()} for instance).
     */
    public static void addStartupTimestampsToIntent(Intent intent) {
        intent.putExtra(EXTRA_STARTUP_REALTIME_MS, SystemClock.elapsedRealtime());
        intent.putExtra(EXTRA_STARTUP_UPTIME_MS, SystemClock.uptimeMillis());
    }

    /**
     * @return the startup timestamp associated with an intent in the elapsedRealtime timebase, or
     *         -1.
     */
    public static long getStartupRealtimeMillis(Intent intent) {
        return intent.getLongExtra(EXTRA_STARTUP_REALTIME_MS, -1);
    }

    /**
     * @return the startup timestamp associated with an intent in the uptimeMillis timebase, or -1.
     */
    public static long getStartupUptimeMillis(Intent intent) {
        return intent.getLongExtra(EXTRA_STARTUP_UPTIME_MS, -1);
    }
}
