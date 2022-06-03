// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.Process;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.annotations.MainDex;

/**
 * Kills and (optionally) restarts the main Chrome process, then immediately kills itself.
 *
 * Starting this Activity should only be done by the
 * {@link org.chromium.chrome.browser.init.ChromeLifetimeController}, and requires
 * passing in the process ID (the Intent should have the value of Process#myPid() as an extra).
 *
 * This Activity runs on a separate process from the main Chrome browser and cannot see the main
 * process' Activities.  It works around an Android framework issue for alarms set via the
 * AlarmManager, which requires a minimum alarm duration of 5 seconds: https://crbug.com/515919.
 */
@MainDex // Runs in a separate process.
public class BrowserRestartActivity extends Activity {
    public static final String EXTRA_MAIN_PID =
            "org.chromium.chrome.browser.BrowserRestartActivity.main_pid";
    public static final String EXTRA_RESTART =
            "org.chromium.chrome.browser.BrowserRestartActivity.restart";

    /**
     * Creates an Intent to start the {@link BrowserRestartActivity}.  Must only be called by the
     * {@link org.chromium.chrome.browser.init.ChromeLifetimeController}.
     * @param context       Context to use when constructing the Intent.
     * @param restartChrome Whether or not to restart Chrome after killing the process.
     * @return Intent that can be used to restart Chrome.
     */
    public static Intent createIntent(Context context, boolean restartChrome) {
        Intent intent = new Intent();
        intent.setClassName(context.getPackageName(), BrowserRestartActivity.class.getName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(BrowserRestartActivity.EXTRA_MAIN_PID, Process.myPid());
        intent.putExtra(BrowserRestartActivity.EXTRA_RESTART, restartChrome);
        return intent;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Kill the main Chrome process.
        Intent intent = getIntent();
        int mainBrowserPid = IntentUtils.safeGetIntExtra(
                intent, BrowserRestartActivity.EXTRA_MAIN_PID, -1);
        assert mainBrowserPid != -1;
        assert mainBrowserPid != Process.myPid();
        Process.killProcess(mainBrowserPid);

        // Fire an Intent to restart Chrome, if necessary.
        boolean restart = IntentUtils.safeGetBooleanExtra(
                intent, BrowserRestartActivity.EXTRA_RESTART, false);
        if (restart) {
            Context context = ContextUtils.getApplicationContext();
            Intent restartIntent = new Intent(Intent.ACTION_MAIN);
            restartIntent.setPackage(context.getPackageName());
            restartIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            context.startActivity(restartIntent);
        }

        // Kill this process.
        finish();
        Process.killProcess(Process.myPid());
    }
}
