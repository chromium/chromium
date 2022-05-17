// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.ActivityManager.RecentTaskInfo;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.SystemClock;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.TrustedWebUtils;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.instantapps.InstantAppsHandler;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.translate.TranslateIntentHandler;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.chrome.browser.webapps.WebappLauncherActivity;
import org.chromium.components.browser_ui.media.MediaNotificationUma;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.widget.Toast;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Set;

/**
 * Dispatches incoming intents to the appropriate activity based on the current configuration and
 * Intent fired.
 */
public class LaunchIntentDispatcher implements IntentHandler.IntentHandlerDelegate {
    /**
     * Extra indicating launch mode used.
     */
    public static final String EXTRA_LAUNCH_MODE =
            "com.google.android.apps.chrome.EXTRA_LAUNCH_MODE";

    private static final String TAG = "ActivitiyDispatcher";

    private final Activity mActivity;
    private Intent mIntent;

    @IntDef({Action.CONTINUE, Action.FINISH_ACTIVITY, Action.FINISH_ACTIVITY_REMOVE_TASK})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Action {
        int CONTINUE = 0;
        int FINISH_ACTIVITY = 1;
        int FINISH_ACTIVITY_REMOVE_TASK = 2;
    }

    /**
     * Dispatches the intent in the context of the activity.
     * In most cases calling this method will result in starting a new activity, in which case
     * the current activity will need to be finished as per the action returned.
     *
     * @param currentActivity activity that received the intent
     * @param intent intent to dispatch
     * @return action to take
     */
    public static @Action int dispatch(Activity currentActivity, Intent intent) {
        return new LaunchIntentDispatcher(currentActivity, intent).dispatch();
    }

    /**
     * Dispatches the intent to proper tabbed activity.
     * This method is similar to {@link #dispatch()}, but only handles intents that result in
     * starting a tabbed activity (i.e. one of *TabbedActivity classes).
     *
     * @param currentActivity activity that received the intent
     * @param intent intent to dispatch
     * @return action to take
     */
    public static @Action int dispatchToTabbedActivity(Activity currentActivity, Intent intent) {
        return new LaunchIntentDispatcher(currentActivity, intent).dispatchToTabbedActivity();
    }

    private LaunchIntentDispatcher(Activity activity, Intent intent) {
        mActivity = activity;
        mIntent = IntentUtils.sanitizeIntent(intent);

        // Needs to be called as early as possible, to accurately capture the
        // time at which the intent was received.
        if (mIntent != null && IntentHandler.getTimestampFromIntent(mIntent) == -1) {
            IntentHandler.addTimestampToIntent(mIntent);
        }

        recordIntentMetrics();
    }

    /**
     * Figure out how to route the Intent.  Because this is on the critical path to startup, please
     * avoid making the pathway any more complicated than it already is.  Make sure that anything
     * you add _absolutely has_ to be here.
     */
    private @Action int dispatch() {
        // Read partner browser customizations information asynchronously.
        // We want to initialize early because when there are no tabs to restore, we should possibly
        // show homepage, which might require reading PartnerBrowserCustomizations provider.
        PartnerBrowserCustomizations.getInstance().initializeAsync(
                mActivity.getApplicationContext());

        int tabId = IntentHandler.getBringTabToFrontId(mIntent);
        boolean incognito =
                mIntent.getBooleanExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, false);

        // Check if a web search Intent is being handled.
        IntentHandler intentHandler = new IntentHandler(mActivity, this);
        String url = IntentHandler.getUrlFromIntent(mIntent);
        if (url == null && tabId == Tab.INVALID_TAB_ID && !incognito
                && intentHandler.handleWebSearchIntent(mIntent)) {
            return Action.FINISH_ACTIVITY;
        }

        // Check if a LIVE WebappActivity has to be brought back to the foreground.  We can't
        // check for a dead WebappActivity because we don't have that information without a global
        // TabManager.  If that ever lands, code to bring back any Tab could be consolidated
        // here instead of being spread between ChromeTabbedActivity and ChromeLauncherActivity.
        // https://crbug.com/443772, https://crbug.com/522918
        if (WebappLauncherActivity.bringWebappToFront(tabId)) {
            return Action.FINISH_ACTIVITY_REMOVE_TASK;
        }

        // Check if we should launch an Instant App to handle the intent.
        if (InstantAppsHandler.getInstance().handleIncomingIntent(
                    mActivity, mIntent, false, false)) {
            return Action.FINISH_ACTIVITY;
        }

        return dispatchToTabbedActivity();
    }

    @Override
    public void processWebSearchIntent(String query) {
    }

    @Override
    public void processTranslateTabIntent(
            @Nullable String targetLanguageCode, @Nullable String expectedUrl) {
        assert false;
    }

    @Override
    public void processUrlViewIntent(LoadUrlParams loadUrlParams, int tabOpenType,
            String externalAppId, int tabIdToBringToFront, Intent intent) {
        assert false;
    }

    @Override
    public long getIntentHandlingTimeMs() {
        assert false;
        return 0;
    }

    /** When started with an intent, maybe pre-resolve the domain. */
    private void maybePrefetchDnsInBackground() {
        if (mIntent != null && Intent.ACTION_VIEW.equals(mIntent.getAction())) {
            String maybeUrl = IntentHandler.getUrlFromIntent(mIntent);
            if (maybeUrl != null) {
                WarmupManager.getInstance().maybePrefetchDnsForUrlInBackground(mActivity, maybeUrl);
            }
        }
    }

    /**
     * Adds a token to TRANSLATE_TAB intents that we know were sent from a first party app.
     *
     * TRANSLATE_TAB requires a signature permission. We know that permission has been enforced (and
     * thus comes from a first party application) if it was routed via the TranslateDispatcher
     * activity-alias. In this case, add a token so IntentHandler knows the intent is from a first
     * party app.
     */
    private static void maybeAuthenticateFirstPartyTranslateIntent(Intent intent) {
        if (intent != null && TranslateIntentHandler.ACTION_TRANSLATE_TAB.equals(intent.getAction())
                && TranslateIntentHandler.COMPONENT_TRANSLATE_DISPATCHER.equals(
                        intent.getComponent().getClassName())) {
            IntentUtils.addTrustedIntentExtras(intent);
        }
    }

    /**
     * @return Whether the intent is for launching a Custom Tab.
     */
    public static boolean isCustomTabIntent(Intent intent) {
        if (intent == null) return false;
        if (CustomTabsIntent.shouldAlwaysUseBrowserUI(intent)
                || !intent.hasExtra(CustomTabsIntent.EXTRA_SESSION)) {
            return false;
        }
        return IntentHandler.getUrlFromIntent(intent) != null;
    }

    /**
     * Handles launching a {@link ChromeTabbedActivity}.
     */
    @SuppressLint("InlinedApi")
    @SuppressWarnings("checkstyle:SystemExitCheck") // Allowed due to https://crbug.com/847921#c17.
    private @Action int dispatchToTabbedActivity() {

        maybePrefetchDnsInBackground();

        maybeAuthenticateFirstPartyTranslateIntent(mIntent);

        Intent newIntent = new Intent(mIntent);

        if (Intent.ACTION_VIEW.equals(newIntent.getAction())
                && !IntentHandler.wasIntentSenderChrome(newIntent)) {
            long time = SystemClock.elapsedRealtime();
            if (!chromeTabbedTaskExists()) {
                newIntent.putExtra(IntentHandler.EXTRA_STARTED_TABBED_CHROME_TASK, true);
            }
            RecordHistogram.recordTimesHistogram("Startup.Android.ChromeTabbedTaskExistsTime",
                    SystemClock.elapsedRealtime() - time);
        }

        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP_MR1) {
            Uri extraReferrer = mActivity.getReferrer();
            if (extraReferrer != null) {
                newIntent.putExtra(IntentHandler.EXTRA_ACTIVITY_REFERRER, extraReferrer.toString());
            }
        }

        String targetActivityClassName = MultiWindowUtils.getInstance()
                                                 .getTabbedActivityForIntent(newIntent, mActivity)
                                                 .getName();
        newIntent.setClassName(
                mActivity.getApplicationContext().getPackageName(), targetActivityClassName);
        newIntent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_NEW_TASK
                | Intent.FLAG_ACTIVITY_RETAIN_IN_RECENTS);
        Uri uri = newIntent.getData();
        boolean isContentScheme = false;
        if (uri != null && UrlConstants.CONTENT_SCHEME.equals(uri.getScheme())) {
            isContentScheme = true;
            newIntent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        }
        if (MultiWindowUtils.getInstance().shouldRunInLegacyMultiInstanceMode(mActivity, mIntent)) {
            MultiWindowUtils.getInstance().makeLegacyMultiInstanceIntent(mActivity, newIntent);
        }

        if (newIntent.getComponent().getClassName().equals(mActivity.getClass().getName())) {
            // We're trying to start activity that is already running - just continue.
            return Action.CONTINUE;
        }

        // This system call is often modified by OEMs and not actionable. http://crbug.com/619646.
        try {
            Bundle options = null;
            mActivity.startActivity(newIntent, options);
        } catch (SecurityException ex) {
            if (isContentScheme) {
                Toast.makeText(mActivity,
                             org.chromium.chrome.R.string.external_app_restricted_access_error,
                             Toast.LENGTH_LONG)
                        .show();
            } else {
                throw ex;
            }
        }

        return Action.FINISH_ACTIVITY;
    }

    private boolean chromeTabbedTaskExists() {
        // Fast check for a running Chrome instance.
        for (Activity activity : ApplicationStatus.getRunningActivities()) {
            if (activity instanceof ChromeTabbedActivity) return true;
        }
        // Slightly slower check for an existing task (One IPC, usually ~2ms).
        try {
            Set<RecentTaskInfo> recentTaskInfos =
                    AndroidTaskUtils.getRecentTaskInfosMatchingComponentNames(
                            mActivity, ChromeTabbedActivity.TABBED_MODE_COMPONENT_NAMES);
            return !recentTaskInfos.isEmpty();
        } catch (SecurityException ex) {
            // If we can't query task status, assume a Chrome task exists so this doesn't
            // mistakenly lead to a Chrome task being removed.
            return true;
        }
    }

    /**
     * Records metrics gleaned from the Intent.
     */
    private void recordIntentMetrics() {
        @IntentHandler.ExternalAppId
        int source = IntentHandler.determineExternalIntentSource(mIntent);
        if (mIntent.getPackage() == null && source != IntentHandler.ExternalAppId.CHROME) {
            int flagsOfInterest = Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT;
            int maskedFlags = mIntent.getFlags() & flagsOfInterest;
            RecordHistogram.recordSparseHistogram("Launch.IntentFlags", maskedFlags);
        }
        MediaNotificationUma.recordClickSource(mIntent);
    }

    private static boolean clearTopIntentsForCustomTabsEnabled(Intent intent) {
        // The new behavior is important for TWAs, but could potentially affect other clients.
        // For now we expose this risky change only to TWAs.
        return IntentUtils.safeGetBooleanExtra(
                intent, TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, false);
    }
}
