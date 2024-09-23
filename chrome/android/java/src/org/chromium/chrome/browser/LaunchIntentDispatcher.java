// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.ActivityManager.RecentTaskInfo;
import android.app.Notification;
import android.app.SearchManager;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Bundle;
import android.provider.MediaStore;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.OptIn;
import androidx.browser.auth.ExperimentalAuthTab;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.browser.customtabs.TrustedWebUtils;
import androidx.core.os.BuildCompat;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.browserservices.SessionDataHolder;
import org.chromium.chrome.browser.browserservices.ui.splashscreen.trustedwebactivity.TwaSplashController;
import org.chromium.chrome.browser.customtabs.AuthTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.firstrun.FirstRunFlowSequencer;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.intents.BrowserIntentUtils;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.notifications.NotificationPlatformBridge;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.searchwidget.SearchActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.chrome.browser.webapps.WebappLauncherActivity;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.widget.Toast;
import org.chromium.webapk.lib.common.WebApkConstants;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Set;

/**
 * Dispatches incoming intents to the appropriate activity based on the current configuration and
 * Intent fired.
 */
public class LaunchIntentDispatcher {
    /** Extra indicating launch mode used. */
    public static final String EXTRA_LAUNCH_MODE =
            "com.google.android.apps.chrome.EXTRA_LAUNCH_MODE";

    private static final String TAG = "ActivityDispatcher";

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

    /**
     * Dispatches the intent to proper tabbed activity.
     * This method is similar to {@link #dispatch()}, but only handles intents that result in
     * starting a custom tab activity.
     */
    public static @Action int dispatchToCustomTabActivity(Activity currentActivity, Intent intent) {
        LaunchIntentDispatcher dispatcher = new LaunchIntentDispatcher(currentActivity, intent);
        if (!isCustomTabIntent(dispatcher.mIntent)) return Action.CONTINUE;
        if (dispatcher.launchCustomTabActivity()) {
            return Action.FINISH_ACTIVITY;
        } else {
            return Action.CONTINUE;
        }
    }

    private LaunchIntentDispatcher(Activity activity, Intent intent) {
        mActivity = activity;
        mIntent = IntentUtils.sanitizeIntent(intent);

        // Needs to be called as early as possible, to accurately capture the
        // time at which the intent was received.
        if (mIntent != null && BrowserIntentUtils.getStartupRealtimeMillis(mIntent) == -1) {
            BrowserIntentUtils.addStartupTimestampsToIntent(mIntent);
        }
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
        PartnerBrowserCustomizations.getInstance()
                .initializeAsync(mActivity.getApplicationContext());

        boolean isCustomTabIntent = isCustomTabIntent(mIntent);

        int tabId = IntentHandler.getBringTabToFrontId(mIntent);
        boolean incognito =
                mIntent.getBooleanExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, false);

        String url = IntentHandler.getUrlFromIntent(mIntent);

        // Check if a web search Intent is being handled.
        if (url == null
                && tabId == Tab.INVALID_TAB_ID
                && !incognito
                && processWebSearchIntent(mIntent)) {
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

        // Check if we should push the user through First Run.
        if (FirstRunFlowSequencer.launch(mActivity, mIntent, /* preferLightweightFre= */ false)) {
            return Action.FINISH_ACTIVITY;
        }

        // Check if we should launch a Custom Tab.
        if (isCustomTabIntent) {
            launchCustomTabActivity();
            return Action.FINISH_ACTIVITY;
        }

        // b(357902796): Handle fall-back path for unbound WebAPKs.
        if (isWebApkIntent(mIntent) && launchWebApk()) {
            return Action.FINISH_ACTIVITY;
        }

        return dispatchToTabbedActivity();
    }

    private boolean processWebSearchIntent(Intent intent) {
        if (intent == null) return false;

        String query = null;
        final String action = intent.getAction();
        if (Intent.ACTION_SEARCH.equals(action)
                || MediaStore.INTENT_ACTION_MEDIA_SEARCH.equals(action)) {
            query = IntentUtils.safeGetStringExtra(intent, SearchManager.QUERY);
        }
        if (TextUtils.isEmpty(query)) return false;

        // Only the ChromeLauncherActivity can handle search intents. Drop the intent and abort the
        // launch.
        if (!(mActivity instanceof ChromeLauncherActivity)) return true;

        Intent searchIntent = new Intent(Intent.ACTION_WEB_SEARCH);
        searchIntent.putExtra(SearchManager.QUERY, query);

        if (PackageManagerUtils.canResolveActivity(
                searchIntent, PackageManager.GET_RESOLVED_FILTER)) {
            mActivity.startActivity(searchIntent);
        } else {
            // Phone doesn't have a WEB_SEARCH action handler, open Search Activity with
            // the given query.
            Intent searchActivityIntent = new Intent(Intent.ACTION_MAIN);
            searchActivityIntent.setClass(
                    ContextUtils.getApplicationContext(), SearchActivity.class);
            searchActivityIntent.putExtra(SearchManager.QUERY, query);
            mActivity.startActivity(searchActivityIntent);
        }
        return true;
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
    @OptIn(markerClass = ExperimentalAuthTab.class)
    public static boolean isCustomTabIntent(Intent intent) {
        if (intent == null) return false;
        Log.w(
                TAG,
                "CustomTabsIntent#shouldAlwaysUseBrowserUI() = "
                        + CustomTabsIntent.shouldAlwaysUseBrowserUI(intent));
        if (CustomTabsIntent.shouldAlwaysUseBrowserUI(intent)
                || (!intent.hasExtra(CustomTabsIntent.EXTRA_SESSION)
                        && !AuthTabIntentDataProvider.isAuthTabIntent(intent))) {
            return false;
        }
        return IntentHandler.getUrlFromIntent(intent) != null;
    }

    private static boolean isWebApkIntent(Intent intent) {
        return intent != null && intent.hasExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME);
    }

    /** Creates an Intent that can be used to launch a {@link CustomTabActivity}. */
    public static Intent createCustomTabActivityIntent(Context context, Intent intent) {
        // Use the copy constructor to carry over the myriad of extras.
        Uri uri = Uri.parse(IntentHandler.getUrlFromIntent(intent));

        Intent newIntent = new Intent(intent);
        newIntent.setAction(Intent.ACTION_VIEW);
        newIntent.setData(uri);
        newIntent.setClassName(context, CustomTabActivity.class.getName());
        // Make sure the result of the CustomTabActivity is forwarded to the client.
        newIntent.addFlags(Intent.FLAG_ACTIVITY_FORWARD_RESULT);

        // Since configureIntentForResizableCustomTab() might change the componenet/class
        // associated with the passed intent, it needs to be called after #setClassName(context,
        // CustomTabActivity.class.getName());
        CustomTabIntentDataProvider.configureIntentForResizableCustomTab(context, newIntent);

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
                newIntent.addFlags(
                        Intent.FLAG_ACTIVITY_SINGLE_TOP | Intent.FLAG_ACTIVITY_CLEAR_TOP);
            }
        }

        // If |uri| is a content:// URI, we want to propagate the URI permissions. This can't be
        // achieved by simply adding the FLAG_GRANT_READ_URI_PERMISSION to the Intent, since the
        // data URI on the Intent isn't |uri|, it just has |uri| as a query parameter.
        if (uri != null && UrlConstants.CONTENT_SCHEME.equals(uri.getScheme())) {
            String packageName = context.getPackageName();
            try {
                context.grantUriPermission(packageName, uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
            } catch (Exception e) {
                // SecurityException or UndeclaredThrowableException.
                // https://crbug.com/1373209
                Log.w(TAG, "Unable to grant Uri permission", e);
            }
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

            // Adjacent launch flag, if present, already would have made the launcher activity start
            // on the adajcent screen in multi-window mode. Clear it on the new Intent for the flag
            // to take effect only once.
            newIntent.setFlags(newIntent.getFlags() & ~Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT);

            // Android will try to find and reuse an existing CCT activity in the background. Use
            // this flag to always start a new one instead.
            newIntent.addFlags(Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
            // Force a new document to ensure the proper task/stack creation.
            newIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
        }

        return newIntent;
    }

    private static SessionDataHolder getSessionDataHolder() {
        return ChromeApplicationImpl.getComponent().resolveSessionDataHolder();
    }

    /**
     * Handles launching a {@link CustomTabActivity}, which will sit on top of a client's activity
     * in the same task. Returns whether an Activity was launched (or brought to the foreground).
     */
    private boolean launchCustomTabActivity() {
        CustomTabsConnection.getInstance()
                .onHandledIntent(
                        CustomTabsSessionToken.getSessionTokenFromIntent(mIntent), mIntent);

        boolean isCustomTab = true;
        if (IntentHandler.shouldIgnoreIntent(mIntent, mActivity, isCustomTab)) {
            return false;
        }

        if (!clearTopIntentsForCustomTabsEnabled(mIntent)) {
            // The old way of delivering intents relies on calling the activity directly via a
            // static reference. It doesn't allow using CLEAR_TOP, and also doesn't work when an
            // intent brings the task to foreground. The condition above is a temporary safety net.
            boolean handled = getSessionDataHolder().handleIntent(mIntent);
            if (handled) return true;
        }
        maybePrefetchDnsInBackground();

        // Strip EXTRA_CALLING_ACTIVITY_PACKAGE/EXTRA_LAUNCHED_FROM_PACKAGE if present on
        // the original intent so that it cannot be spoofed by CCT client apps.
        IntentUtils.safeRemoveExtra(mIntent, IntentHandler.EXTRA_CALLING_ACTIVITY_PACKAGE);
        IntentUtils.safeRemoveExtra(mIntent, IntentHandler.EXTRA_LAUNCHED_FROM_PACKAGE);

        Intent intent = new Intent(mIntent);
        String packageName = mActivity.getCallingPackage(); // from startActivityForResult
        String packageNameIdentitySharing = getCallingPackageIdentitySharing();
        if (packageName == null) packageName = packageNameIdentitySharing;
        if (packageName != null) {
            intent.putExtra(IntentHandler.EXTRA_CALLING_ACTIVITY_PACKAGE, packageName);
        }

        // Pass the package name obtained via identity sharing API separately from the one
        // obtained via startActivityForResult.
        boolean identityShared = packageNameIdentitySharing != null;
        if (identityShared) {
            intent.putExtra(IntentHandler.EXTRA_LAUNCHED_FROM_PACKAGE, packageNameIdentitySharing);
        }
        // Create and fire a launch intent.
        Intent launchIntent = createCustomTabActivityIntent(mActivity, intent);
        Uri extraReferrer = mActivity.getReferrer();
        if (extraReferrer != null) {
            launchIntent.putExtra(IntentHandler.EXTRA_ACTIVITY_REFERRER, extraReferrer.toString());
        }

        // Allow disk writes during startActivity() to avoid strict mode violations on some
        // Samsung devices, see https://crbug.com/796548.
        if (TwaSplashController.handleIntent(mActivity, launchIntent)) {
            return true;
        }

        mActivity.startActivity(launchIntent, null);
        RecordHistogram.recordBooleanHistogram("CustomTabs.IdentityShared", identityShared);
        return true;
    }

    private boolean launchWebApk() {
        // TODO(crbug.com/357902796): it may be possible to save 20ms or so by calling into
        // WebappLauncherActivity code directly instead of sending an intent.

        Intent webApkIntent = new Intent(WebappLauncherActivity.ACTION_START_WEBAPP);
        webApkIntent.setPackage(mActivity.getPackageName());

        webApkIntent.setFlags(mIntent.getFlags());

        Bundle copiedExtras = mIntent.getExtras();
        if (copiedExtras != null) {
            webApkIntent.putExtras(copiedExtras);
        }

        try {
            mActivity.startActivity(webApkIntent);
        } catch (ActivityNotFoundException e) {
            Log.w(TAG, "Unable to launch browser in WebAPK mode.");
            RecordHistogram.recordBooleanHistogram("WebApk.LaunchFromViewIntent", false);
            return false;
        }

        RecordHistogram.recordBooleanHistogram("WebApk.LaunchFromViewIntent", true);
        return true;
    }

    /**
     * Returns client package name obtained from {@link Activity#getLaunchedFromPackage()}. {@code
     * null} if the underlying OS doesn't support the feature.
     */
    private String getCallingPackageIdentitySharing() {
        return BuildCompat.isAtLeastU() ? mActivity.getLaunchedFromPackage() : null;
    }

    /** Handles launching a {@link ChromeTabbedActivity}. */
    @SuppressLint("InlinedApi")
    @SuppressWarnings("checkstyle:SystemExitCheck") // Allowed due to https://crbug.com/847921#c17.
    private @Action int dispatchToTabbedActivity() {
        maybePrefetchDnsInBackground();

        Intent newIntent = new Intent(mIntent);

        if (Intent.ACTION_VIEW.equals(newIntent.getAction())
                && !IntentHandler.wasIntentSenderChrome(newIntent)) {
            if (!chromeTabbedTaskExists()) {
                newIntent.putExtra(IntentHandler.EXTRA_STARTED_TABBED_CHROME_TASK, true);
            }
            if ((newIntent.getFlags() & Intent.FLAG_ACTIVITY_NEW_TASK) != 0) {
                // Adjacent launch flag, if present, already would have made the launcher activity
                // start on the adajcent screen in multi-window mode. Clear it on the new Intent for
                // the flag to take effect only once.
                newIntent.setFlags(newIntent.getFlags() & ~Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT);
            }
            RecordHistogram.recordBooleanHistogram(
                    "Android.Intent.HasNonSpoofablePackageName", hasNonSpoofablePackageName());
            boolean identityShared = getCallingPackageIdentitySharing() != null;
            RecordHistogram.recordBooleanHistogram("Android.Intent.IdentityShared", identityShared);
        }

        if (mActivity instanceof ChromeLauncherActivity) {
            newIntent.putExtra(IntentHandler.EXTRA_LAUNCHED_VIA_CHROME_LAUNCHER_ACTIVITY, true);
        }

        Uri extraReferrer = mActivity.getReferrer();
        if (extraReferrer != null) {
            newIntent.putExtra(IntentHandler.EXTRA_ACTIVITY_REFERRER, extraReferrer.toString());
        }

        String targetActivityClassName =
                MultiWindowUtils.getInstance()
                        .getTabbedActivityForIntent(newIntent, mActivity)
                        .getName();
        newIntent.setClassName(
                mActivity.getApplicationContext().getPackageName(), targetActivityClassName);
        newIntent.setFlags(
                Intent.FLAG_ACTIVITY_CLEAR_TOP
                        | Intent.FLAG_ACTIVITY_NEW_TASK
                        | Intent.FLAG_ACTIVITY_RETAIN_IN_RECENTS);

        // If the source of an intent containing FLAG_ACTIVITY_MULTIPLE_TASK is Chrome, retain the
        // flag to support multi-instance launch.
        if (IntentUtils.isTrustedIntentFromSelf(mIntent)
                && (mIntent.getFlags() & Intent.FLAG_ACTIVITY_MULTIPLE_TASK) != 0) {
            newIntent.setFlags(newIntent.getFlags() | Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
        }

        Uri uri = newIntent.getData();
        boolean isContentScheme = false;
        if (uri != null && UrlConstants.CONTENT_SCHEME.equals(uri.getScheme())) {
            isContentScheme = true;
            newIntent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        }

        if (newIntent.getComponent().getClassName().equals(mActivity.getClass().getName())) {
            // We're trying to start activity that is already running - just continue.
            return Action.CONTINUE;
        }

        // This system call is often modified by OEMs and not actionable. http://crbug.com/619646.
        try {
            mActivity.startActivity(newIntent);
        } catch (SecurityException ex) {
            if (isContentScheme) {
                Toast.makeText(
                                mActivity,
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

    private boolean hasNonSpoofablePackageName() {
        return !TextUtils.isEmpty(mActivity.getCallingPackage())
                || !TextUtils.isEmpty(getCallingPackageIdentitySharing());
    }

    private static boolean clearTopIntentsForCustomTabsEnabled(Intent intent) {
        // The new behavior is important for TWAs, but could potentially affect other clients.
        // For now we expose this risky change only to TWAs.
        return IntentUtils.safeGetBooleanExtra(
                intent, TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, false);
    }
}
