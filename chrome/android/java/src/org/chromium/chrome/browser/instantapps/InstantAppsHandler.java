// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.instantapps;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.Build;
import android.os.SystemClock;
import android.provider.Browser;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.CachedMetrics.EnumeratedHistogramSample;
import org.chromium.base.metrics.CachedMetrics.TimesHistogramSample;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.externalnav.ExternalNavigationDelegateImpl;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.content_public.browser.WebContents;

/** A launcher for Instant Apps. */
public class InstantAppsHandler {
    private static final String TAG = "InstantAppsHandler";

    private static final Object INSTANCE_LOCK = new Object();
    private static InstantAppsHandler sInstance;

    private static final String CUSTOM_APPS_INSTANT_APP_EXTRA =
            "android.support.customtabs.extra.EXTRA_ENABLE_INSTANT_APPS";

    private static final String INSTANT_APP_START_TIME_EXTRA =
            "org.chromium.chrome.INSTANT_APP_START_TIME";

    // TODO(mariakhomenko): Use system once we roll to O SDK.
    private static final int FLAG_DO_NOT_LAUNCH = 0x00000200;

    // TODO(mariakhomenko): Depend directly on the constants once we roll to v8 libraries.
    private static final String DO_NOT_LAUNCH_EXTRA =
            "com.google.android.gms.instantapps.DO_NOT_LAUNCH_INSTANT_APP";

    protected static final String IS_REFERRER_TRUSTED_EXTRA =
            "com.google.android.gms.instantapps.IS_REFERRER_TRUSTED";

    protected static final String IS_USER_CONFIRMED_LAUNCH_EXTRA =
            "com.google.android.gms.instantapps.IS_USER_CONFIRMED_LAUNCH";

    protected static final String TRUSTED_REFERRER_PKG_EXTRA =
            "com.google.android.gms.instantapps.TRUSTED_REFERRER_PKG";

    public static final String IS_GOOGLE_SEARCH_REFERRER =
            "com.google.android.gms.instantapps.IS_GOOGLE_SEARCH_REFERRER";

    private static final String BROWSER_LAUNCH_REASON =
            "com.google.android.gms.instantapps.BROWSER_LAUNCH_REASON";

    private static final String SUPERVISOR_PKG = "com.google.android.instantapps.supervisor";

    private static final String[] SUPERVISOR_START_ACTIONS = {
            "com.google.android.instantapps.START", "com.google.android.instantapps.nmr1.INSTALL",
            "com.google.android.instantapps.nmr1.VIEW"};

    // Instant Apps system resolver activity on N-MR1+.
    @VisibleForTesting
    public static final String EPHEMERAL_INSTALLER_CLASS =
            "com.google.android.gms.instantapps.routing.EphemeralInstallerActivity";

    /** Finch experiment name. */
    private static final String INSTANT_APPS_EXPERIMENT_NAME = "InstantApps";

    /** Finch experiment group which is enabled for instant apps. */
    private static final String INSTANT_APPS_ENABLED_ARM = "InstantAppsEnabled";

    /** Finch experiment group which is disabled for instant apps. */
    private static final String INSTANT_APPS_DISABLED_ARM = "InstantAppsDisabled";

    /** A histogram to record how long each handleIntent() call took. */
    private static final TimesHistogramSample sHandleIntentDuration =
            new TimesHistogramSample("Android.InstantApps.HandleIntentDuration");

    /** A histogram to record how long the fallback intent roundtrip was. */
    private static final TimesHistogramSample sFallbackIntentTimes =
            new TimesHistogramSample("Android.InstantApps.FallbackDuration");

    // Only two possible call sources for fallback intents, set boundary at n+1.
    private static final int SOURCE_BOUNDARY = 3;

    private static final EnumeratedHistogramSample sFallbackCallSource =
            new EnumeratedHistogramSample("Android.InstantApps.CallSource", SOURCE_BOUNDARY);

    /**
     * A histogram to record how long the GMS Core API call took when the instant app was found.
     */
    private static final TimesHistogramSample sInstantAppsApiCallTimesHasApp =
            new TimesHistogramSample("Android.InstantApps.ApiCallDurationWithApp");

    /**
     * A histogram to record how long the GMS Core API call took when the instant app was not found.
     */
    private static final TimesHistogramSample sInstantAppsApiCallTimesNoApp =
            new TimesHistogramSample("Android.InstantApps.ApiCallDurationWithoutApp");

    /** @return The singleton instance of {@link InstantAppsHandler}. */
    public static InstantAppsHandler getInstance() {
        synchronized (INSTANCE_LOCK) {
            if (sInstance == null) {
                sInstance = AppHooks.get().createInstantAppsHandler();
            }
        }
        return sInstance;
    }

    /**
     * Checks whether {@param intent} is for an Instant App. Considers both package and actions that
     * would resolve to Supervisor.
     * @return Whether the given intent is going to open an Instant App.
     */
    public static boolean isIntentToInstantApp(Intent intent) {
        if (SUPERVISOR_PKG.equals(intent.getPackage())) return true;

        String intentAction = intent.getAction();
        for (String action : SUPERVISOR_START_ACTIONS) {
            if (action.equals(intentAction)) {
                return true;
            }
        }
        return false;
    }

    /**
     * Record how long the handleIntent() method took.
     * @param startTime The timestamp for handleIntent start time.
     */
    private void recordHandleIntentDuration(long startTime) {
        sHandleIntentDuration.record(SystemClock.elapsedRealtime() - startTime);
    }

    /**
     * Record the amount of time spent in the Instant Apps API call.
     * @param startTime The time at which we started doing computations.
     * @param hasApp Whether the API has found an Instant App during the call.
     */
    protected void recordInstantAppsApiCallTime(long startTime, boolean hasApp) {
        if (hasApp) {
            sInstantAppsApiCallTimesHasApp.record(SystemClock.elapsedRealtime() - startTime);
        } else {
            sInstantAppsApiCallTimesNoApp.record(SystemClock.elapsedRealtime() - startTime);
        }
    }

    /**
     * In the case where Chrome is called through the fallback mechanism from Instant Apps,
     * record the amount of time the whole trip took and which UI took the user back to Chrome,
     * if any.
     * @param intent The current intent.
     */
    private void maybeRecordFallbackStats(Intent intent) {
        Long startTime = IntentUtils.safeGetLongExtra(intent, INSTANT_APP_START_TIME_EXTRA, 0);
        if (startTime > 0) {
            sFallbackIntentTimes.record(SystemClock.elapsedRealtime() - startTime);
            intent.removeExtra(INSTANT_APP_START_TIME_EXTRA);
        }
        int callSource = IntentUtils.safeGetIntExtra(intent, BROWSER_LAUNCH_REASON, 0);
        if (callSource > 0 && callSource < SOURCE_BOUNDARY) {
            sFallbackCallSource.record(callSource);
            intent.removeExtra(BROWSER_LAUNCH_REASON);
        } else if (callSource >= SOURCE_BOUNDARY) {
            Log.e(TAG, "Unexpected call source constant for Instant Apps: " + callSource);
        }
    }

    /**
     * Handle incoming intent.
     * @param context Context.
     * @param intent The incoming intent being handled.
     * @param isCustomTabsIntent Whether we are in custom tabs.
     * @param isRedirect Whether this is the redirect resolve case where incoming intent was
     *        resolved to another URL.
     * @return Whether Instant Apps is handling the URL request.
     */
    public boolean handleIncomingIntent(Context context, Intent intent,
            boolean isCustomTabsIntent, boolean isRedirect) {
        long startTimeStamp = SystemClock.elapsedRealtime();
        boolean result = handleIncomingIntentInternal(context, intent, isCustomTabsIntent,
                startTimeStamp, isRedirect);
        recordHandleIntentDuration(startTimeStamp);
        return result;
    }

    private boolean handleIncomingIntentInternal(
            Context context, Intent intent, boolean isCustomTabsIntent, long startTime,
            boolean isRedirect) {
        if (!isRedirect && !isCustomTabsIntent && Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            Log.i(TAG, "Package manager handles intents on O+, not handling in Chrome");
            return false;
        }

        if (isCustomTabsIntent && !IntentUtils.safeGetBooleanExtra(
                intent, CUSTOM_APPS_INSTANT_APP_EXTRA, false)) {
            Log.i(TAG, "Not handling with Instant Apps (missing CUSTOM_APPS_INSTANT_APP_EXTRA)");
            return false;
        }

        if (IntentUtils.safeGetBooleanExtra(intent, DO_NOT_LAUNCH_EXTRA, false)
                || (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
                           && (intent.getFlags() & FLAG_DO_NOT_LAUNCH) != 0)) {
            maybeRecordFallbackStats(intent);
            Log.i(TAG, "Not handling with Instant Apps (DO_NOT_LAUNCH_EXTRA)");
            return false;
        }

        if (IntentUtils.safeGetBooleanExtra(
                intent, IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, false)
                || IntentUtils.safeHasExtra(intent, ShortcutHelper.EXTRA_SOURCE)
                || isIntentFromChrome(context, intent)
                || (IntentHandler.getUrlFromIntent(intent) == null)) {
            Log.i(TAG, "Not handling with Instant Apps (other)");
            return false;
        }

        // Used to search for the intent handlers. Needs null component to return correct results.
        Intent intentCopy = new Intent(intent);
        intentCopy.setComponent(null);
        Intent selector = intentCopy.getSelector();
        if (selector != null) selector.setComponent(null);

        if (!(isCustomTabsIntent || isChromeDefaultHandler(context))
                || ExternalNavigationDelegateImpl.isPackageSpecializedHandler(null, intentCopy)) {
            // Chrome is not the default browser or a specialized handler exists.
            Log.i(TAG, "Not handling with Instant Apps because Chrome is not default or "
                    + "there's a specialized handler");
            return false;
        }

        Intent callbackIntent = new Intent(intent);
        callbackIntent.putExtra(DO_NOT_LAUNCH_EXTRA, true);
        callbackIntent.putExtra(INSTANT_APP_START_TIME_EXTRA, startTime);

        return tryLaunchingInstantApp(context, intent, isCustomTabsIntent, callbackIntent);
    }

    /**
     * Attempts to launch an Instant App, if possible.
     * @param context The activity context.
     * @param intent The incoming intent.
     * @param isCustomTabsIntent Whether the intent is for a CustomTab.
     * @param fallbackIntent The intent that will be launched by Instant Apps in case of failure to
     *        load.
     * @return Whether an Instant App was launched.
     */
    protected boolean tryLaunchingInstantApp(
            Context context, Intent intent, boolean isCustomTabsIntent, Intent fallbackIntent) {
        return false;
    }

    /**
     * Evaluate a navigation for whether it should launch an Instant App or show the Instant
     * App banner.
     * @return Whether an Instant App intent was started.
     */
    public boolean handleNavigation(Context context, String url, Uri referrer, Tab tab) {
        boolean urlIsInstantAppDefault =
                InstantAppsSettings.isInstantAppDefault(tab.getWebContents(), url);
        if (shouldLaunchInstantApp(tab.getWebContents(), url, referrer, urlIsInstantAppDefault)) {
            return launchInstantAppForNavigation(context, url, referrer);
        }
        maybeShowInstantAppBanner(context, url, referrer, tab, urlIsInstantAppDefault);
        return false;
    }

    /**
     * Returns whether or not we should launch an instant app immediately for the given URL.
     *
     * @param webContents A {@link WebContents}.
     * @param url The URL we might launch an instant app for.
     * @param referrer The referring URL.
     * @return Whether we should launch the instant app.
     */
    private boolean shouldLaunchInstantApp(
            WebContents webContents, String url, Uri referrer, boolean urlIsInstantAppDefault) {
        // Launch the instant app automatically on these conditions:
        // a) The host of the current URL and referrer are different, and the user has chosen to
        //    launch this instant app in the past.
        // b) The host of the current URL and referrer are the same, but the referrer URL isn't
        //    handled by an instant app and the current one is.
        if (!urlIsInstantAppDefault) return false;

        String urlHost = Uri.parse(url).getHost();
        boolean sameHosts =
                referrer != null && urlHost != null && urlHost.equals(referrer.getHost());
        return (sameHosts && getInstantAppIntentForUrl(referrer.toString()) == null) || !sameHosts;
    }

    /**
     * Shows an Instant App banner if necessary for the page we're loading.
     *
     * @param context An Android {@link Context}.
     * @param url The URL we're navigating to.
     * @param referrer The referrer {@link Uri}.
     * @param tab A Chrome {@link Tab}.
     * @param isInstantAppDefault Whether this instant app is being opened by default.
     */
    protected void maybeShowInstantAppBanner(
            Context context, String url, Uri referrer, Tab tab, boolean isInstantAppDefault) {}

    /**
     * Launches an Instant App immediately, if possible.
     */
    protected boolean launchInstantAppForNavigation(Context context, String url, Uri referrer) {
        return false;
    }

    /**
     * @return Whether the intent was fired from Chrome. This happens when the user gets a
     *         disambiguation dialog and chooses to stay in Chrome.
     */
    private boolean isIntentFromChrome(Context context, Intent intent) {
        return context.getPackageName().equals(IntentUtils.safeGetStringExtra(
                intent, Browser.EXTRA_APPLICATION_ID))
                // We shouldn't leak internal intents with authentication tokens
                || IntentHandler.wasIntentSenderChrome(intent);
    }

    /** @return Whether Chrome is the default browser on the device. */
    private boolean isChromeDefaultHandler(Context context) {
        return SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.CHROME_DEFAULT_BROWSER, false);
    }

    /**
     * Launches the Instant App from the infobar banner.
     */
    public void launchFromBanner(InstantAppsBannerData data) {
        if (data.getIntent() == null) return;

        Intent iaIntent = data.getIntent();
        if (data.getReferrer() != null) {
            iaIntent.putExtra(Intent.EXTRA_REFERRER, data.getReferrer());
            iaIntent.putExtra(IS_REFERRER_TRUSTED_EXTRA, true);
        }

        Context appContext = ContextUtils.getApplicationContext();
        iaIntent.putExtra(TRUSTED_REFERRER_PKG_EXTRA, appContext.getPackageName());
        iaIntent.putExtra(IS_USER_CONFIRMED_LAUNCH_EXTRA, true);

        try {
            appContext.startActivity(iaIntent);
            InstantAppsSettings.setInstantAppDefault(data.getWebContents(), data.getUrl());
        } catch (Exception e) {
            Log.e(TAG, "Could not launch instant app intent", e);
        }
    }

    /**
     * Gets the instant app intent for the given URL if one exists.
     *
     * @param url The URL whose instant app this is associated with.
     * @return An instant app intent for the URL if one exists.
     */
    public Intent getInstantAppIntentForUrl(String url) {
        return null;
    }

    /**
     * Returns whether or not the instant app is available.
     *
     * @param url The URL where the instant app is located.
     * @param checkHoldback Check if the app would be available if the user weren't in the holdback
     *        group.
     * @param includeUserPrefersBrowser Function should return true if there's an instant app intent
     *        even if the user has opted out of instant apps.
     * @return Whether or not the instant app specified by the entry in the page's manifest is
     *         either available, or would be available if the user wasn't in the holdback group.
     */
    public boolean isInstantAppAvailable(
            String url, boolean checkHoldback, boolean includeUserPrefersBrowser) {
        return false;
    }

    /**
     * Whether the given ResolveInfo object refers to Instant Apps as a launcher.
     * @param info The resolve info.
     */
    public boolean isInstantAppResolveInfo(ResolveInfo info) {
        if (info == null) return false;

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            return info.isInstantAppAvailable;
        } else if (info.activityInfo != null) {
            return EPHEMERAL_INSTALLER_CLASS.equals(info.activityInfo.name);
        }

        return false;
    }
}
