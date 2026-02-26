// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.ActivityManager.RecentTaskInfo;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.TrustedWebUtils;
import androidx.core.os.BuildCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.CommandLine;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.SessionDataHolder;
import org.chromium.chrome.browser.browserservices.SessionHandler;
import org.chromium.chrome.browser.browserservices.intents.SessionHolder;
import org.chromium.chrome.browser.browserservices.ui.splashscreen.trustedwebactivity.TwaSplashController;
import org.chromium.chrome.browser.customtabs.AuthTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.ResolutionType;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.widget.Toast;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Set;

/**
 * Dispatches incoming intents to the appropriate activity based on the current configuration and
 * Intent fired.
 */
@NullMarked
public class LaunchIntentDispatcher {
    /** Extra indicating launch mode used. */
    public static final String EXTRA_LAUNCH_MODE =
            "com.google.android.apps.chrome.EXTRA_LAUNCH_MODE";

    private static final String TAG = "ActivityDispatcher";

    private final Activity mActivity;
    private final Intent mIntent;

    @IntDef({Action.CONTINUE, Action.FINISH_ACTIVITY, Action.FINISH_ACTIVITY_REMOVE_TASK})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Action {
        int CONTINUE = 0;
        int FINISH_ACTIVITY = 1;
        int FINISH_ACTIVITY_REMOVE_TASK = 2;
    }

    /**
     * Dispatches the intent to proper tabbed activity. Only handles intents that result in starting
     * a tabbed activity (i.e. one of *TabbedActivity classes).
     *
     * @param currentActivity activity that received the intent
     * @param intent intent to dispatch
     * @return action to take
     */
    public static @Action int dispatchToTabbedActivity(Activity currentActivity, Intent intent) {
        return new LaunchIntentDispatcher(currentActivity, intent).dispatchToTabbedActivity();
    }

    /**
     * Dispatches the intent to Search Activity.
     *
     * @param client SearchActivityClient instance
     * @param currentActivity activity that received the intent
     * @param intent intent to dispatch
     * @return action to take
     */
    public static @Action int dispatchToSearchActivity(
            SearchActivityClient client, Activity currentActivity, Intent intent) {
        client.requestOmniboxForResult(
                client.newIntentBuilder()
                        .setResolutionType(ResolutionType.OPEN_OR_LAUNCH_CHROME)
                        .build());
        return Action.FINISH_ACTIVITY;
    }

    /**
     * Dispatches the intent to CustomTabActivity if the itent is a valid CustomTabActivity intent.
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
        boolean unparcelFds = ChromeFeatureList.sUnparcelIntentFileDescriptors.isEnabled();
        mIntent = assertNonNull(IntentUtils.sanitizeIntent(intent, unparcelFds));
    }

    /** When started with an intent, maybe pre-resolve the domain. */
    private void maybePrefetchDnsInBackground() {
        if (Intent.ACTION_VIEW.equals(mIntent.getAction())) {
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

    /** Creates an Intent that can be used to launch a {@link CustomTabActivity}. */
    public static Intent createCustomTabActivityIntent(Context context, Intent intent) {
        // Use the copy constructor to carry over the myriad of extras.
        String uriString = IntentHandler.getUrlFromIntent(intent);
        assumeNonNull(uriString);
        Uri uri = Uri.parse(uriString);

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
                    SessionDataHolder.getInstance()
                            .getActiveHandlerClassInCurrentTask(intent, context);
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

    /**
     * Handles launching a {@link CustomTabActivity}, which will sit on top of a client's activity
     * in the same task. Returns whether an Activity was launched (or brought to the foreground).
     */
    private boolean launchCustomTabActivity() {
        CustomTabsConnection.getInstance()
                .onHandledIntent(SessionHolder.getSessionHolderFromIntent(mIntent), mIntent);

        boolean isCustomTab = true;
        if (IntentHandler.shouldIgnoreIntent(mIntent, mActivity, isCustomTab)) {
            return false;
        }

        if (!clearTopIntentsForCustomTabsEnabled(mIntent)) {
            // The old way of delivering intents relies on calling the activity directly via a
            // static reference. It doesn't allow using CLEAR_TOP, and also doesn't work when an
            // intent brings the task to foreground. The condition above is a temporary safety net.
            SessionHandler handler =
                    SessionDataHolder.getInstance().getActiveHandlerForIntent(mIntent);
            if (handler != null && handler.handleIntent(mIntent)) {
                // Bring the task to the foreground.
                // This is necessary because LaunchIntentDispatcher bypasses startActivity (which
                // usually handles foregrounding) when it delegates to this handler.
                ApiCompatibilityUtils.moveTaskToFront(mActivity, handler.getTaskId(), 0);
                return true;
            }
        }

        // Should not be set by external apps, remove if present.
        mIntent.removeExtra(IntentHandler.EXTRA_CCT_EARLY_NAV);
        boolean startedNavigationEarly = maybeStartNavigation();
        RecordHistogram.recordBooleanHistogram(
                "CustomTabs.Startup.StartedNavigationEarly", startedNavigationEarly);
        if (!startedNavigationEarly) maybePrefetchDnsInBackground();

        Intent intent = new Intent(mIntent);
        boolean identityShared = maybePutCallingAppPackage(intent);

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

    // Pass the target Activity the package name of the calling app.
    // EXTRA_LAUNCHED_FROM_PACKAGE: set only when identity sharing is enabled by the calling app
    // EXTRA_CALLING_ACTIVITY_PACKAGE: from either startActivityForResult or identity sharing
    private boolean maybePutCallingAppPackage(Intent intent) {
        // Strip EXTRA_CALLING_ACTIVITY_PACKAGE/EXTRA_LAUNCHED_FROM_PACKAGE if present on
        // the original intent so that it cannot be spoofed by CCT client apps.
        IntentUtils.safeRemoveExtra(intent, IntentHandler.EXTRA_CALLING_ACTIVITY_PACKAGE);
        IntentUtils.safeRemoveExtra(intent, IntentHandler.EXTRA_LAUNCHED_FROM_PACKAGE);

        String packageName = mActivity.getCallingPackage();
        String packageNameIdentitySharing = getCallingPackageIdentitySharing();
        if (packageName == null) packageName = packageNameIdentitySharing;
        if (packageName != null) {
            intent.putExtra(IntentHandler.EXTRA_CALLING_ACTIVITY_PACKAGE, packageName);
        }
        boolean hasIdentitySharingPackageName = packageNameIdentitySharing != null;
        if (hasIdentitySharingPackageName) {
            intent.putExtra(IntentHandler.EXTRA_LAUNCHED_FROM_PACKAGE, packageNameIdentitySharing);
        }
        return hasIdentitySharingPackageName;
    }

    private boolean maybeStartNavigation() {
        if (!ProfileManager.isInitialized()) return false;
        if (IntentHandler.willLaunchIncognitoCustomTab(mIntent)) return false;
        if (clearTopIntentsForCustomTabsEnabled(mIntent)
                && SessionDataHolder.getInstance()
                                .getActiveHandlerClassInCurrentTask(mIntent, mActivity)
                        != null) {
            return false;
        }
        // Not opening into an existing Activity, can start navigation early if a spare tab
        // exists.
        Profile profile = ProfileManager.getLastUsedRegularProfile();
        return CustomTabsConnection.getInstance().startEarlyNavigationInHiddenTab(profile, mIntent);
    }

    /**
     * Returns client package name obtained from {@link Activity#getLaunchedFromPackage()}. {@code
     * null} if the underlying OS doesn't support the feature.
     */
    private @Nullable String getCallingPackageIdentitySharing() {
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
            if (!chromeTabbedTaskExists(mActivity)) {
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
            boolean identityShared = maybePutCallingAppPackage(newIntent);
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

        String className = assumeNonNull(newIntent.getComponent()).getClassName();
        assumeNonNull(className);
        if (className.equals(mActivity.getClass().getName())) {
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
                                R.string.external_app_restricted_access_error,
                                Toast.LENGTH_LONG)
                        .show();
            } else {
                throw ex;
            }
        }

        return Action.FINISH_ACTIVITY;
    }

    /**
     * Checks if a Chrome tabbed task currently exists, either in the foreground or background.
     *
     * @param context The application context.
     * @return whether a Chrome tabbed task is found (either running or in recent tasks).
     */
    public static boolean chromeTabbedTaskExists(Context context) {
        // Fast check for a running Chrome instance.
        for (Activity activity : ApplicationStatus.getRunningActivities()) {
            if (activity instanceof ChromeTabbedActivity) return true;
        }
        // Slightly slower check for an existing task (One IPC, usually ~2ms).
        try {
            Set<RecentTaskInfo> recentTaskInfos =
                    AndroidTaskUtils.getRecentTaskInfosMatchingComponentNames(
                            context, ChromeTabbedActivity.TABBED_MODE_COMPONENT_NAMES);
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
