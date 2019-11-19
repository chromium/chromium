// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.app.Activity;
import android.app.PendingIntent;
import android.app.PendingIntent.CanceledException;
import android.content.Intent;
import android.os.Bundle;

import org.chromium.base.Log;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.metrics.UmaUtils;
import org.chromium.chrome.browser.profiles.ProfileManagerUtils;
import org.chromium.chrome.browser.util.IntentUtils;

/** Base class for First Run Experience. */
public abstract class FirstRunActivityBase extends AsyncInitializationActivity {
    private static final String TAG = "FirstRunActivity";

    public static final String EXTRA_COMING_FROM_CHROME_ICON = "Extra.ComingFromChromeIcon";
    public static final String EXTRA_CHROME_LAUNCH_INTENT_IS_CCT =
            "Extra.FreChromeLaunchIntentIsCct";

    // The intent to send once the FRE completes.
    public static final String EXTRA_FRE_COMPLETE_LAUNCH_INTENT = "Extra.FreChromeLaunchIntent";

    // The extras on the intent which initiated first run. (e.g. the extras on the intent
    // received by ChromeLauncherActivity.)
    public static final String EXTRA_CHROME_LAUNCH_INTENT_EXTRAS =
            "Extra.FreChromeLaunchIntentExtras";

    static final String SHOW_WELCOME_PAGE = "ShowWelcome";
    static final String SHOW_DATA_REDUCTION_PAGE = "ShowDataReduction";
    static final String SHOW_SEARCH_ENGINE_PAGE = "ShowSearchEnginePage";
    static final String SHOW_SIGNIN_PAGE = "ShowSignIn";

    public static final boolean DEFAULT_METRICS_AND_CRASH_REPORTING = true;

    private boolean mNativeInitialized;

    @Override
    protected boolean requiresFirstRunToBeCompleted(Intent intent) {
        // The user is already in First Run.
        return false;
    }

    @Override
    public boolean shouldStartGpuProcess() {
        return true;
    }

    // Activity:

    @Override
    public void onPause() {
        super.onPause();
        UmaUtils.recordBackgroundTime();
        flushPersistentData();
    }

    @Override
    public void onResume() {
        super.onResume();
        // Since the FRE may be shown before any tab is shown, mark that this is the point at
        // which Chrome went to foreground. This is needed as otherwise an assert will be hit
        // in UmaUtils.getForegroundStartTicks() when recording the time taken to load the first
        // page (which happens after native has been initialized possibly while FRE is still
        // active).
        UmaUtils.recordForegroundStartTime();
    }

    @Override
    public void finishNativeInitialization() {
        super.finishNativeInitialization();
        mNativeInitialized = true;
    }

    protected void flushPersistentData() {
        if (mNativeInitialized) {
            ProfileManagerUtils.flushPersistentDataForAllProfiles();
        }
    }

    /**
     * Sends PendingIntent included with the EXTRA_FRE_COMPLETE_LAUNCH_INTENT extra.
     * @return Whether a pending intent was sent.
     */
    protected final boolean sendFirstRunCompletePendingIntent() {
        PendingIntent pendingIntent =
                IntentUtils.safeGetParcelableExtra(getIntent(), EXTRA_FRE_COMPLETE_LAUNCH_INTENT);
        boolean pendingIntentIsCCT = IntentUtils.safeGetBooleanExtra(
                getIntent(), EXTRA_CHROME_LAUNCH_INTENT_IS_CCT, false);
        if (pendingIntent == null) return false;

        try {
            PendingIntent.OnFinished onFinished = null;
            if (pendingIntentIsCCT) {
                // After the PendingIntent has been sent, send a first run callback to custom tabs
                // if necessary.
                onFinished = new PendingIntent.OnFinished() {
                    @Override
                    public void onSendFinished(PendingIntent pendingIntent, Intent intent,
                            int resultCode, String resultData, Bundle resultExtras) {
                        // Use {@link FirstRunActivityBase#getIntent()} instead of {@link intent}
                        // parameter in order to use a more similar code path for completing first
                        // run and for aborting first run.
                        notifyCustomTabCallbackFirstRunIfNecessary(getIntent(), true);
                    }
                };
            }

            // Use the PendingIntent to send the intent that originally launched Chrome. The intent
            // will go back to the ChromeLauncherActivity, which will route it accordingly.
            pendingIntent.send(Activity.RESULT_OK, onFinished, null);
            // Use fade-out animation for the transition from this activity so the original intent.
            overridePendingTransition(android.R.anim.fade_in, android.R.anim.fade_out);
            return true;
        } catch (CanceledException e) {
            Log.e(TAG, "Unable to send PendingIntent.", e);
        }
        return false;
    }

    /**
     * If the first run activity was triggered by a custom tab, notify app associated with
     * custom tab whether first run was completed.
     * @param freIntent First run activity intent.
     * @param complete  Whether first run completed successfully.
     */
    public static void notifyCustomTabCallbackFirstRunIfNecessary(
            Intent freIntent, boolean complete) {
        boolean launchedByCCT = IntentUtils.safeGetBooleanExtra(
                freIntent, EXTRA_CHROME_LAUNCH_INTENT_IS_CCT, false);
        if (!launchedByCCT) return;

        Bundle launchIntentExtras =
                IntentUtils.safeGetBundleExtra(freIntent, EXTRA_CHROME_LAUNCH_INTENT_EXTRAS);
        CustomTabsConnection.getInstance().sendFirstRunCallbackIfNecessary(
                launchIntentExtras, complete);
    }
}
