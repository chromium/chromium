// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.Notification;
import android.app.SearchManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.StrictMode;

import androidx.annotation.IntDef;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.browser.customtabs.TrustedWebUtils;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.metrics.CachedMetrics;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.browserservices.SessionDataHolder;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.splashscreen.TwaSplashController;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.PaymentHandlerActivity;
import org.chromium.chrome.browser.customtabs.SeparateTaskCustomTabActivity;
import org.chromium.chrome.browser.firstrun.FirstRunFlowSequencer;
import org.chromium.chrome.browser.instantapps.InstantAppsHandler;
import org.chromium.chrome.browser.metrics.MediaNotificationUma;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.notifications.NotificationPlatformBridge;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.searchwidget.SearchActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.browser.vr.VrModuleProvider;
import org.chromium.chrome.browser.webapps.ActivityAssigner;
import org.chromium.chrome.browser.webapps.WebappLauncherActivity;
import org.chromium.ui.widget.Toast;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.UUID;

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

    /**
     * Timeout in ms for reading PartnerBrowserCustomizations provider. We do not trust third party
     * provider by default.
     */
    private static final int PARTNER_BROWSER_CUSTOMIZATIONS_TIMEOUT_MS = 10000;

    private static final CachedMetrics.SparseHistogramSample sIntentFlagsHistogram =
            new CachedMetrics.SparseHistogramSample("Launch.IntentFlags");

    private final Activity mActivity;
    private final Intent mIntent;
    private final boolean mIsCustomTabIntent;
    private final boolean mIsVrIntent;

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

    /**
     * Dispatches the intent to proper tabbed activity.
     * This method is similar to {@link #dispatch()}, but only handles intents that result in
     * starting a custom tab activity.
     */
    public static @Action int dispatchToCustomTabActivity(Activity currentActivity, Intent intent) {
        LaunchIntentDispatcher dispatcher = new LaunchIntentDispatcher(currentActivity, intent);
        if (!dispatcher.mIsCustomTabIntent) return Action.CONTINUE;
        dispatcher.launchCustomTabActivity();
        return Action.FINISH_ACTIVITY;
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

        mIsVrIntent = VrModuleProvider.getIntentDelegate().isVrIntent(mIntent);
        mIsCustomTabIntent = isCustomTabIntent(mIntent);
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
        PartnerBrowserCustomizations.initializeAsync(
                mActivity.getApplicationContext(), PARTNER_BROWSER_CUSTOMIZATIONS_TIMEOUT_MS);

        int tabId = IntentUtils.safeGetIntExtra(
                mIntent, IntentHandler.TabOpenType.BRING_TAB_TO_FRONT_STRING, Tab.INVALID_TAB_ID);
        boolean incognito =
                mIntent.getBooleanExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, false);

        // Check if a web search Intent is being handled.
        IntentHandler intentHandler = new IntentHandler(this, mActivity.getPackageName());
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

        // The notification settings cog on the flipped side of Notifications and in the Android
        // Settings "App Notifications" view will open us with a specific category.
        if (mIntent.hasCategory(Notification.INTENT_CATEGORY_NOTIFICATION_PREFERENCES)) {
            NotificationPlatformBridge.launchNotificationPreferences(mIntent);
            return Action.FINISH_ACTIVITY;
        }

        // Check if we should launch an Instant App to handle the intent.
        if (InstantAppsHandler.getInstance().handleIncomingIntent(
                    mActivity, mIntent, mIsCustomTabIntent, false)) {
            return Action.FINISH_ACTIVITY;
        }

        // Check if we should push the user through First Run.
        if (FirstRunFlowSequencer.launch(mActivity, mIntent, false /* requiresBroadcast */,
                    false /* preferLightweightFre */)) {
            return Action.FINISH_ACTIVITY;
        }

        // Check if we should launch a Custom Tab.
        if (mIsCustomTabIntent) {
            launchCustomTabActivity();
            return Action.FINISH_ACTIVITY;
        }

        return dispatchToTabbedActivity();
    }

    @Override
    public void processWebSearchIntent(String query) {
        Intent searchIntent = new Intent(Intent.ACTION_WEB_SEARCH);
        searchIntent.putExtra(SearchManager.QUERY, query);

        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            int resolvers =
                    PackageManagerUtils
                            .queryIntentActivities(searchIntent, PackageManager.GET_RESOLVED_FILTER)
                            .size();
            if (resolvers == 0) {
                // Phone doesn't have a WEB_SEARCH action handler, open Search Activity with
                // the given query.
                Intent searchActivityIntent = new Intent(Intent.ACTION_MAIN);
                searchActivityIntent.setClass(
                        ContextUtils.getApplicationContext(), SearchActivity.class);
                searchActivityIntent.putExtra(SearchManager.QUERY, query);
                mActivity.startActivity(searchActivityIntent);
            } else {
                mActivity.startActivity(searchIntent);
            }
        }
    }

    @Override
    public void processUrlViewIntent(String url, String referer, String headers,
            @IntentHandler.TabOpenType int tabOpenType, String externalAppId,
            int tabIdToBringToFront, boolean hasUserGesture, Intent intent) {
        assert false;
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
     * Creates an Intent that can be used to launch a {@link CustomTabActivity}.
     */
    public static Intent createCustomTabActivityIntent(Context context, Intent intent) {
        // Use the copy constructor to carry over the myriad of extras.
        Uri uri = Uri.parse(IntentHandler.getUrlFromIntent(intent));

        Intent newIntent = new Intent(intent);
        newIntent.setAction(Intent.ACTION_VIEW);
        newIntent.setData(uri);
        newIntent.setClassName(context, CustomTabActivity.class.getName());

        if (clearTopIntentsForCustomTabsEnabled(intent)) {
            // Ensure the new intent is routed into the instance of CustomTabActivity in this task.
            // If the existing CustomTabActivity can't handle the intent, it will re-launch
            // the intent without these flags.
            // If you change this flow, please make sure it works correctly with
            // - "Don't keep activities",
            // - Multiple clients hosting CCTs,
            // - Multiwindow mode.
            Class<? extends Activity> handlerClass =
                    getSessionDataHolder().getActiveHandlerClassInCurrentTask(intent, context);
            if (handlerClass != null) {
                newIntent.setClassName(context, handlerClass.getName());
                newIntent.addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP |
                        Intent.FLAG_ACTIVITY_CLEAR_TOP);
            }
        }

        // Use a custom tab with a unique theme for payment handlers.
        if (intent.getIntExtra(CustomTabIntentDataProvider.EXTRA_UI_TYPE, CustomTabsUiType.DEFAULT)
                == CustomTabsUiType.PAYMENT_REQUEST) {
            newIntent.setClassName(context, PaymentHandlerActivity.class.getName());
        }

        // If |uri| is a content:// URI, we want to propagate the URI permissions. This can't be
        // achieved by simply adding the FLAG_GRANT_READ_URI_PERMISSION to the Intent, since the
        // data URI on the Intent isn't |uri|, it just has |uri| as a query parameter.
        if (uri != null && UrlConstants.CONTENT_SCHEME.equals(uri.getScheme())) {
            context.grantUriPermission(
                    context.getPackageName(), uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        }

        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.OPEN_CUSTOM_TABS_IN_NEW_TASK)) {
            newIntent.setFlags(newIntent.getFlags() | Intent.FLAG_ACTIVITY_NEW_TASK);
        }

        // Handle activity started in a new task.
        // See https://developer.android.com/guide/components/activities/tasks-and-back-stack
        if ((newIntent.getFlags() & Intent.FLAG_ACTIVITY_NEW_TASK) != 0
                || (newIntent.getFlags() & Intent.FLAG_ACTIVITY_NEW_DOCUMENT) != 0) {
            // If a CCT intent triggers First Run, then NEW_TASK will be automatically applied. As
            // part of that, it will inherit the EXCLUDE_FROM_RECENTS bit from
            // ChromeLauncherActivity, so explicitly remove it to ensure the CCT does not get lost
            // in recents.
            newIntent.setFlags(newIntent.getFlags() & ~Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS);

            // Android will try to find and reuse an existing CCT activity in the background. Use
            // this flag to always start a new one instead.
            newIntent.addFlags(Intent.FLAG_ACTIVITY_MULTIPLE_TASK);

            // Provide the general feeling of supporting multi tasks in Android version that did not
            // fully support them. Reuse the least recently used SeparateTaskCustomTabActivity
            // instance.
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
                String uuid = UUID.randomUUID().toString();
                int activityIndex = ActivityAssigner
                                            .instance(ActivityAssigner.ActivityAssignerNamespace
                                                              .SEPARATE_TASK_CCT_NAMESPACE)
                                            .assign(uuid);
                String className = SeparateTaskCustomTabActivity.class.getName() + activityIndex;
                newIntent.setClassName(context, className);

                String url = IntentHandler.getUrlFromIntent(newIntent);
                assert url != null;
                newIntent.setData(new Uri.Builder()
                                          .scheme(UrlConstants.CUSTOM_TAB_SCHEME)
                                          .authority(uuid)
                                          .query(url)
                                          .build());
            } else {
                // Force a new document to ensure the proper task/stack creation.
                newIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
            }
        }

        // If the previous caller was not Chrome, but added EXTRA_IS_OPENED_BY_CHROME or
        // EXTRA_IS_OPENED_BY_WEBAPK for malicious purpose, remove it. The new intent will be sent
        // by Chrome, but was not sent by Chrome initially.
        if (!IntentHandler.wasIntentSenderChrome(intent)) {
            IntentUtils.safeRemoveExtra(
                    newIntent, CustomTabIntentDataProvider.EXTRA_IS_OPENED_BY_CHROME);
            IntentUtils.safeRemoveExtra(
                    newIntent, CustomTabIntentDataProvider.EXTRA_IS_OPENED_BY_WEBAPK);
        }

        return newIntent;
    }

    private static SessionDataHolder getSessionDataHolder() {
        return ChromeApplication.getComponent().resolveSessionDataHolder();
    }

    /**
     * Handles launching a {@link CustomTabActivity}, which will sit on top of a client's activity
     * in the same task.
     */
    private void launchCustomTabActivity() {
        CustomTabsConnection.getInstance().onHandledIntent(
                CustomTabsSessionToken.getSessionTokenFromIntent(mIntent), mIntent);
        if (!clearTopIntentsForCustomTabsEnabled(mIntent)) {
            // The old way of delivering intents relies on calling the activity directly via a
            // static reference. It doesn't allow using CLEAR_TOP, and also doesn't work when an
            // intent brings the task to foreground. The condition above is a temporary safety net.
            boolean handled = getSessionDataHolder().handleIntent(mIntent);
            if (handled) return;
        }
        maybePrefetchDnsInBackground();

        // Create and fire a launch intent.
        Intent launchIntent = createCustomTabActivityIntent(mActivity, mIntent);

        // Allow disk writes during startActivity() to avoid strict mode violations on some
        // Samsung devices, see https://crbug.com/796548.
        try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            if (TwaSplashController.handleIntent(mActivity, launchIntent)) {
                return;
            }

            mActivity.startActivity(launchIntent, null);
        }
    }

    /**
     * Handles launching a {@link ChromeTabbedActivity}.
     */
    @SuppressLint("InlinedApi")
    @SuppressWarnings("checkstyle:SystemExitCheck") // Allowed due to https://crbug.com/847921#c17.
    private @Action int dispatchToTabbedActivity() {
        if (mIsVrIntent) {
            for (Activity activity : ApplicationStatus.getRunningActivities()) {
                if (activity instanceof ChromeTabbedActivity) {
                    if (VrModuleProvider.getDelegate().willChangeDensityInVr(
                                (ChromeActivity) activity)) {
                        // In the rare case that entering VR will trigger a density change (and
                        // hence an Activity recreation), just return to Daydream home and kill the
                        // process, as there's no good way to recreate without showing 2D UI
                        // in-headset.
                        mActivity.finish();
                        System.exit(0);
                    }
                }
            }
        }

        maybePrefetchDnsInBackground();

        Intent newIntent = new Intent(mIntent);
        String targetActivityClassName = MultiWindowUtils.getInstance()
                                                 .getTabbedActivityForIntent(newIntent, mActivity)
                                                 .getName();
        newIntent.setClassName(
                mActivity.getApplicationContext().getPackageName(), targetActivityClassName);
        newIntent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_NEW_TASK);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            newIntent.addFlags(Intent.FLAG_ACTIVITY_RETAIN_IN_RECENTS);
        }
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
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();
        try {
            Bundle options = mIsVrIntent
                    ? VrModuleProvider.getIntentDelegate().getVrIntentOptions(mActivity)
                    : null;
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
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }

        return Action.FINISH_ACTIVITY;
    }

    /**
     * Records metrics gleaned from the Intent.
     */
    private void recordIntentMetrics() {
        @IntentHandler.ExternalAppId
        int source = IntentHandler.determineExternalIntentSource(mIntent);
        if (mIntent.getPackage() == null && source != IntentHandler.ExternalAppId.CHROME) {
            int flagsOfInterest = Intent.FLAG_ACTIVITY_NEW_TASK;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                flagsOfInterest |= Intent.FLAG_ACTIVITY_NEW_DOCUMENT;
            }
            int maskedFlags = mIntent.getFlags() & flagsOfInterest;
            sIntentFlagsHistogram.record(maskedFlags);
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
