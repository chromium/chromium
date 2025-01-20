// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.app.Activity;
import android.app.PendingIntent;
import android.app.PendingIntent.CanceledException;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.os.Bundle;

import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.metrics.SimpleStartupForegroundSessionDetector;
import org.chromium.chrome.browser.metrics.UmaUtils;
import org.chromium.chrome.browser.profiles.ProfileManagerUtils;
import org.chromium.chrome.browser.signin.FullscreenSigninAndHistorySyncActivityBase;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;

/** Base class for First Run Experience. */
public abstract class FirstRunActivityBase extends FullscreenSigninAndHistorySyncActivityBase
        implements BackPressHandler {
    private static final String TAG = "FirstRunActivity";

    public static final String EXTRA_COMING_FROM_CHROME_ICON = "Extra.ComingFromChromeIcon";
    public static final String EXTRA_CHROME_LAUNCH_INTENT_IS_CCT =
            "Extra.FreChromeLaunchIntentIsCct";
    public static final String EXTRA_FRE_INTENT_CREATION_ELAPSED_REALTIME_MS =
            "Extra.FreIntentCreationElapsedRealtimeMs";

    // The intent to send once the FRE completes.
    public static final String EXTRA_FRE_COMPLETE_LAUNCH_INTENT = "Extra.FreChromeLaunchIntent";

    // Use PendingIntent (as opposed to Intent) to start activity after FRE completion.
    // TODO(crbug.com/381107767): Always use Intent. For now, only Custom Tab uses Intent.
    public static final String EXTRA_FRE_USE_PENDING_INTENT = "Extra.FreUsePendingIntent";

    // The extras on the intent which initiated first run. (e.g. the extras on the intent
    // received by ChromeLauncherActivity.)
    public static final String EXTRA_CHROME_LAUNCH_INTENT_EXTRAS =
            "Extra.FreChromeLaunchIntentExtras";
    static final String SHOW_SEARCH_ENGINE_PAGE = "ShowSearchEnginePage";
    static final String SHOW_HISTORY_SYNC_PAGE = "ShowHistorySync";

    public static final boolean DEFAULT_METRICS_AND_CRASH_REPORTING = true;

    private boolean mNativeInitialized;

    @Override
    protected boolean requiresFirstRunToBeCompleted(Intent intent) {
        // The user is already in First Run.
        return false;
    }

    // Activity:
    @Override
    public void onPause() {
        super.onPause();
        // As with onResume() below, for historical reasons the FRE has been able to report
        // background time before post-native initialization, unlike other activities. See
        // http://crrev.com/436530.
        UmaUtils.recordBackgroundTimeWithNative();
        flushPersistentData();
    }

    @Override
    public void onResume() {
        SimpleStartupForegroundSessionDetector.discardSession();
        super.onResume();
        // Since the FRE may be shown before any tab is shown, mark that this is the point at which
        // Chrome went to foreground. Other activities can only
        // recordForegroundStartTimeWithNative() after the post-native initialization has started.
        // See http://crrev.com/436530.
        UmaUtils.recordForegroundStartTimeWithNative();
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
     * Sends Intent (or PendingIntent) included with the EXTRA_FRE_COMPLETE_LAUNCH_INTENT extra.
     *
     * @return Whether an intent was sent.
     */
    protected final boolean sendFirstRunCompleteIntent() {
        boolean usePendingIntent =
                IntentUtils.safeGetBooleanExtra(getIntent(), EXTRA_FRE_USE_PENDING_INTENT, true);
        return usePendingIntent
                ? sendFirstRunCompletePendingIntent()
                : sendFirstRunCompleteOriginalIntent();
    }

    private boolean sendFirstRunCompletePendingIntent() {
        PendingIntent pendingIntent =
                IntentUtils.safeGetParcelableExtra(getIntent(), EXTRA_FRE_COMPLETE_LAUNCH_INTENT);
        if (pendingIntent == null) return false;

        try {
            PendingIntent.OnFinished onFinished = null;
            boolean pendingIntentIsCct =
                    IntentUtils.safeGetBooleanExtra(
                            getIntent(), EXTRA_CHROME_LAUNCH_INTENT_IS_CCT, false);
            if (pendingIntentIsCct) {
                // After the PendingIntent has been sent, send a first run callback to custom tabs
                // if necessary.
                onFinished =
                        new PendingIntent.OnFinished() {
                            @Override
                            public void onSendFinished(
                                    PendingIntent pendingIntent,
                                    Intent intent,
                                    int resultCode,
                                    String resultData,
                                    Bundle resultExtras) {
                                // Use {@link FirstRunActivityBase#getIntent()} instead of {@link
                                // intent} parameter in order to use a more similar code path for
                                // completing first run and for aborting first run.
                                notifyCustomTabCallbackFirstRunIfNecessary(getIntent(), true);
                            }
                        };
            }

            // Use the PendingIntent to send the intent that originally launched Chrome. The intent
            // will go back to the ChromeLauncherActivity, which will route it accordingly.
            pendingIntent.send(Activity.RESULT_OK, onFinished, null);

            // Use fade-out animation for the transition from this activity to the original intent.
            overridePendingTransition(android.R.anim.fade_in, android.R.anim.fade_out);
            return true;
        } catch (CanceledException e) {
            Log.e(TAG, "Unable to send PendingIntent.", e);
        }
        return false;
    }

    /**
     * Sends the original Intent included with the EXTRA_FRE_COMPLETE_LAUNCH_INTENT extra.
     *
     * @return Whether an intent was sent.
     */
    private boolean sendFirstRunCompleteOriginalIntent() {
        Intent intent =
                IntentUtils.safeGetParcelableExtra(getIntent(), EXTRA_FRE_COMPLETE_LAUNCH_INTENT);
        if (intent == null) return false;

        try {
            // Certain types of CCT (namely AuthTab) needs to forward the activity result back
            // to the client app.
            intent.addFlags(Intent.FLAG_ACTIVITY_FORWARD_RESULT);
            intent.setFlags(intent.getFlags() & ~Intent.FLAG_ACTIVITY_NEW_TASK);
            startActivity(intent);

            // Use {@link FirstRunActivityBase#getIntent()} instead of {@link intent} parameter in
            // order to use a more similar code path for completing first run and for aborting
            // first run.
            notifyCustomTabCallbackFirstRunIfNecessary(getIntent(), true);

            // Use fade-out animation for the transition from this activity to the original intent.
            overridePendingTransition(android.R.anim.fade_in, android.R.anim.fade_out);
            return true;
        } catch (ActivityNotFoundException e) {
            Log.e(TAG, "Unable to send Intent.", e);
        }
        return false;
    }

    /**
     * If the first run activity was triggered by a custom tab, notify app associated with custom
     * tab whether first run was completed.
     *
     * @param freIntent First run activity intent.
     * @param complete Whether first run completed successfully.
     */
    public static void notifyCustomTabCallbackFirstRunIfNecessary(
            Intent freIntent, boolean complete) {
        boolean launchedByCct =
                IntentUtils.safeGetBooleanExtra(
                        freIntent, EXTRA_CHROME_LAUNCH_INTENT_IS_CCT, false);
        if (!launchedByCct) return;

        Bundle launchIntentExtras =
                IntentUtils.safeGetBundleExtra(freIntent, EXTRA_CHROME_LAUNCH_INTENT_EXTRAS);
        CustomTabsConnection.getInstance()
                .sendFirstRunCallbackIfNecessary(launchIntentExtras, complete);
    }
}
