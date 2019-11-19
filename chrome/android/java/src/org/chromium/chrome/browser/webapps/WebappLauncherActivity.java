// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.SystemClock;
import android.text.TextUtils;
import android.util.Base64;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.ShortcutSource;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.firstrun.FirstRunFlowSequencer;
import org.chromium.chrome.browser.metrics.LaunchMetrics;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.webapk.lib.client.WebApkValidator;
import org.chromium.webapk.lib.common.WebApkConstants;

import java.lang.ref.WeakReference;

/**
 * Launches web apps.  This was separated from the ChromeLauncherActivity because the
 * ChromeLauncherActivity is not allowed to be excluded from Android's Recents: crbug.com/517426.
 */
public class WebappLauncherActivity extends Activity {
    /**
     * Action fired when an Intent is trying to launch a WebappActivity.
     * Never change the package name or the Intents will fail to launch.
     */
    public static final String ACTION_START_WEBAPP =
            "com.google.android.apps.chrome.webapps.WebappManager.ACTION_START_WEBAPP";

    public static final String SECURE_WEBAPP_LAUNCHER =
            "org.chromium.chrome.browser.webapps.SecureWebAppLauncher";
    public static final String ACTION_START_SECURE_WEBAPP =
            "org.chromium.chrome.browser.webapps.WebappManager.ACTION_START_SECURE_WEBAPP";

    /**
     * Delay in ms for relaunching WebAPK as a result of getting intent with extra
     * {@link WebApkConstants.EXTRA_RELAUNCH}. The delay was chosen arbirtarily and seems to
     * work.
     */
    private static final int WEBAPK_LAUNCH_DELAY_MS = 20;

    private static final String TAG = "webapps";

    /** Creates intent to relaunch WebAPK. */
    public static Intent createRelaunchWebApkIntent(Intent sourceIntent, WebApkInfo webApkInfo) {
        assert webApkInfo != null;

        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(webApkInfo.url()));
        intent.setPackage(webApkInfo.webApkPackageName());
        intent.setFlags(
                Intent.FLAG_ACTIVITY_NEW_TASK | ApiCompatibilityUtils.getActivityNewDocumentFlag());
        Bundle extras = sourceIntent.getExtras();
        if (extras != null) {
            intent.putExtras(extras);
        }
        return intent;
    }

    /**
     * Brings a live WebappActivity back to the foreground if one exists for the given tab ID.
     * @param tabId ID of the Tab to bring back to the foreground.
     * @return True if a live WebappActivity was found, false otherwise.
     */
    public static boolean bringWebappToFront(int tabId) {
        WeakReference<WebappActivity> webappActivity =
                WebappActivity.findWebappActivityWithTabId(tabId);
        if (webappActivity == null || webappActivity.get() == null) return false;
        webappActivity.get().getWebContentsDelegate().activateContents();
        return true;
    }

    /**
     * Generates parameters for the WebAPK first run experience for the given intent. Returns null
     * if the intent does not launch either a WebappLauncherActivity or a WebApkActivity. This
     * method is slow. It makes several PackageManager calls.
     */
    public static @Nullable WebApkInfo maybeSlowlyGenerateWebApkInfoFromIntent(Intent fromIntent) {
        // Check for intents targeted at WebApkActivity, WebApkActivity0-9,
        // SameTaskWebApkActivity and WebappLauncherActivity.
        String targetActivityClassName = fromIntent.getComponent().getClassName();
        if (!targetActivityClassName.startsWith(WebApkActivity.class.getName())
                && !targetActivityClassName.equals(SameTaskWebApkActivity.class.getName())
                && !targetActivityClassName.equals(WebappLauncherActivity.class.getName())) {
            return null;
        }

        return WebApkInfo.create(fromIntent);
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Close the notification tray.
        ContextUtils.getApplicationContext().sendBroadcast(
                new Intent(Intent.ACTION_CLOSE_SYSTEM_DIALOGS));

        long createTimestamp = SystemClock.elapsedRealtime();
        Intent intent = getIntent();

        if (WebappActionsNotificationManager.handleNotificationAction(intent)) {
            finish();
            return;
        }

        ChromeWebApkHost.init();
        WebappInfo webappInfo = tryCreateWebappInfo(intent);

        if (shouldRelaunchWebApk(intent, webappInfo)) {
            relaunchWebApk(this, intent, webappInfo);
            return;
        }

        if (FirstRunFlowSequencer.launch(this, intent, false /* requiresBroadcast */,
                    shouldPreferLightweightFre(webappInfo))) {
            ApiCompatibilityUtils.finishAndRemoveTask(this);
            return;
        }

        if (shouldLaunchWebapp(intent, webappInfo)) {
            launchWebapp(this, intent, webappInfo, createTimestamp);
            return;
        }

        launchInTab(this, intent, webappInfo);
    }

    /**
     * Returns whether to prefer the Lightweight First Run Experience instead of the
     * non-Lightweight First Run Experience when launching the given webapp.
     */
    private static boolean shouldPreferLightweightFre(WebappInfo webappInfo) {
        // Use lightweight FRE for unbound WebAPKs.
        return webappInfo != null && webappInfo.webApkPackageName() != null
                && !webappInfo.webApkPackageName().startsWith(
                        WebApkConstants.WEBAPK_PACKAGE_PREFIX);
    }

    private static boolean shouldLaunchWebapp(Intent intent, WebappInfo webappInfo) {
        // {@link WebApkInfo#create()} and {@link WebappInfo#create()} return null if the intent
        // does not specify required values such as the uri.
        if (webappInfo == null) return false;

        // The component is not exported and can only be launched by Chrome.
        if (intent.getComponent().equals(new ComponentName(
                    ContextUtils.getApplicationContext(), SECURE_WEBAPP_LAUNCHER))) {
            return true;
        }

        String webappUrl = webappInfo.url();
        String webappMac = IntentUtils.safeGetStringExtra(intent, ShortcutHelper.EXTRA_MAC);

        return (webappInfo.isForWebApk() || isValidMacForUrl(webappUrl, webappMac)
                || wasIntentFromChrome(intent));
    }

    private static void launchWebapp(Activity launchingActivity, Intent intent,
            @NonNull WebappInfo webappInfo, long createTimestamp) {
        LaunchMetrics.recordHomeScreenLaunchIntoStandaloneActivity(webappInfo);

        // Add all information needed to launch WebappActivity without {@link
        // WebappActivity#sWebappInfoMap} to launch intent. When the Android OS has killed a
        // WebappActivity and the user selects the WebappActivity from "Android Recents" the
        // WebappActivity is launched without going through WebappLauncherActivity first.
        WebappActivity.addWebappInfo(webappInfo.id(), webappInfo);

        Intent launchIntent = createIntentToLaunchForWebapp(intent, webappInfo, createTimestamp);
        IntentUtils.safeStartActivity(launchingActivity, launchIntent);
        if (IntentUtils.isIntentForNewTaskOrNewDocument(launchIntent)) {
            ApiCompatibilityUtils.finishAndRemoveTask(launchingActivity);
        } else {
            launchingActivity.finish();
            launchingActivity.overridePendingTransition(0, R.anim.no_anim);
        }
    }

    /**
     * Returns whether {@link sourceIntent} was sent by a WebAPK to relaunch itself.
     *
     * A WebAPK sends an intent to Chrome to get relaunched when it knows it is about to get killed
     * as result of a call to PackageManager#setComponentEnabledSetting().
     */
    private static boolean shouldRelaunchWebApk(Intent sourceIntent, WebappInfo webappInfo) {
        return webappInfo != null && webappInfo.isForWebApk()
                && sourceIntent.hasExtra(WebApkConstants.EXTRA_RELAUNCH);
    }

    /** Relaunches WebAPK. */
    private static void relaunchWebApk(
            Activity launchingActivity, Intent sourceIntent, @NonNull WebappInfo info) {
        Intent launchIntent = createRelaunchWebApkIntent(sourceIntent, (WebApkInfo) info);
        launchAfterDelay(
                launchingActivity.getApplicationContext(), launchIntent, WEBAPK_LAUNCH_DELAY_MS);
        ApiCompatibilityUtils.finishAndRemoveTask(launchingActivity);
    }

    /** Extracts start URL from source intent and launches URL in Chrome tab. */
    private static void launchInTab(
            Activity launchingActivity, Intent sourceIntent, WebappInfo webappInfo) {
        Context appContext = ContextUtils.getApplicationContext();
        String webappUrl = IntentUtils.safeGetStringExtra(sourceIntent, ShortcutHelper.EXTRA_URL);
        int webappSource = (webappInfo == null) ? ShortcutSource.UNKNOWN : webappInfo.source();

        if (TextUtils.isEmpty(webappUrl)) return;

        Intent launchIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(webappUrl));
        launchIntent.setClassName(
                appContext.getPackageName(), ChromeLauncherActivity.class.getName());
        launchIntent.putExtra(ShortcutHelper.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);
        launchIntent.putExtra(ShortcutHelper.EXTRA_SOURCE, webappSource);
        launchIntent.setFlags(
                Intent.FLAG_ACTIVITY_NEW_TASK | ApiCompatibilityUtils.getActivityNewDocumentFlag());

        Log.e(TAG, "Shortcut (%s) opened in Chrome.", webappUrl);

        IntentUtils.safeStartActivity(appContext, launchIntent);
        ApiCompatibilityUtils.finishAndRemoveTask(launchingActivity);
    }

    /**
     * Checks whether or not the MAC is present and valid for the web app shortcut.
     *
     * The MAC is used to prevent malicious apps from launching Chrome into a full screen
     * Activity for phishing attacks (among other reasons).
     *
     * @param url The URL for the web app.
     * @param mac MAC to compare the URL against.  See {@link WebappAuthenticator}.
     * @return Whether the MAC is valid for the URL.
     */
    private static boolean isValidMacForUrl(String url, String mac) {
        return mac != null
                && WebappAuthenticator.isUrlValid(url, Base64.decode(mac, Base64.DEFAULT));
    }

    private static boolean wasIntentFromChrome(Intent intent) {
        return IntentHandler.wasIntentSenderChrome(intent);
    }

    /** Returns the class name of the {@link WebappActivity} subclass to launch. */
    private static String selectWebappActivitySubclass(@NonNull WebappInfo info) {
        if (info.isSplashProvidedByWebApk()) {
            return SameTaskWebApkActivity.class.getName();
        }
        String activityName = info.isForWebApk() ? WebApkActivity.class.getName()
                                                 : WebappActivity.class.getName();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
            // Specifically assign the app to a particular WebappActivity instance.
            int namespace = info.isForWebApk()
                    ? ActivityAssigner.ActivityAssignerNamespace.WEBAPK_NAMESPACE
                    : ActivityAssigner.ActivityAssignerNamespace.WEBAPP_NAMESPACE;
            int activityIndex = ActivityAssigner.instance(namespace).assign(info.id());
            activityName += String.valueOf(activityIndex);
        }
        return activityName;
    }

    /**
     * Finds instance of {@link webappActivitySubclass}. Finishes the activity if launching the
     * webapp will:
     * 1) Reuse the currently running activity (activity is singleTask)
     * 2) The currently running activity is for a different webapp than the one being launched (The
     *    {@link ActivityAssigner} has wrapped around.)
     * @param webappActivitySubclass WebappActivity subclass to look for.
     * @param launchWebappId The ID of the webapp being launched.
     */
    private static void finishIfReusingActivity(
            String webappActivitySubclass, String launchWebappId) {
        // {@link #selectWebappActivitySubclass()} does not select singleTask activities on L+.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) return;

        for (Activity activity : ApplicationStatus.getRunningActivities()) {
            if (!activity.getClass().getName().equals(webappActivitySubclass)) {
                continue;
            }
            WebappActivity webappActivity = (WebappActivity) activity;
            if (!TextUtils.equals(webappActivity.getWebappInfo().id(), launchWebappId)) {
                activity.finish();
            }
            break;
        }
    }

    /** Returns intent to launch for the web app. */
    private static Intent createIntentToLaunchForWebapp(
            Intent intent, @NonNull WebappInfo webappInfo, long createTimestamp) {
        String launchActivityClassName = selectWebappActivitySubclass(webappInfo);

        // Finishes the old activity if it has been assigned to a different WebappActivity. See
        // crbug.com/702998.
        finishIfReusingActivity(launchActivityClassName, webappInfo.id());

        Intent launchIntent = new Intent();
        launchIntent.setClassName(ContextUtils.getApplicationContext(), launchActivityClassName);
        webappInfo.setWebappIntentExtras(launchIntent);
        launchIntent.setAction(Intent.ACTION_VIEW);

        // On L+, firing intents with the exact same data should relaunch a particular
        // Activity.
        launchIntent.setData(Uri.parse(WebappActivity.WEBAPP_SCHEME + "://" + webappInfo.id()));

        IntentHandler.addTimestampToIntent(launchIntent, createTimestamp);
        // Pass through WebAPK shell launch timestamp to the new intent.
        WebApkIntentDataProvider.copyWebApkShellLaunchTime(intent, launchIntent);
        WebApkIntentDataProvider.copyNewStyleWebApkSplashShownTime(intent, launchIntent);

        // Setting FLAG_ACTIVITY_CLEAR_TOP handles 2 edge cases:
        // - If a legacy PWA is launching from a notification, we want to ensure that the URL being
        // launched is the URL in the intent. If a paused WebappActivity exists for this id,
        // then by default it will be focused and we have no way of sending the desired URL to
        // it (the intent is swallowed). As a workaround, set the CLEAR_TOP flag to ensure that
        // the existing Activity handles an update via onNewIntent().
        // - If a WebAPK is having a CustomTabActivity on top of it in the same Task, and user
        // clicks a link to takes them back to the scope of a WebAPK, we want to destroy the
        // CustomTabActivity activity and go back to the WebAPK activity. It is intentional that
        // Custom Tab will not be reachable with a back button.
        if (webappInfo.isSplashProvidedByWebApk()) {
            launchIntent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_NO_ANIMATION
                    | Intent.FLAG_ACTIVITY_FORWARD_RESULT);
        } else {
            launchIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK
                    | ApiCompatibilityUtils.getActivityNewDocumentFlag()
                    | Intent.FLAG_ACTIVITY_CLEAR_TOP);
        }

        return launchIntent;
    }

    /** Tries to create WebappInfo/WebApkInfo for the intent. */
    @VisibleForTesting
    static WebappInfo tryCreateWebappInfo(Intent intent) {
        // Builds WebApkInfo for the intent if the WebAPK package specified in the intent is a valid
        // WebAPK and the URL specified in the intent can be fulfilled by the WebAPK.
        String webApkPackage =
                IntentUtils.safeGetStringExtra(intent, WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME);
        String url = IntentUtils.safeGetStringExtra(intent, ShortcutHelper.EXTRA_URL);
        if (!TextUtils.isEmpty(webApkPackage) && !TextUtils.isEmpty(url)
                && WebApkValidator.canWebApkHandleUrl(
                        ContextUtils.getApplicationContext(), webApkPackage, url)) {
            return WebApkInfo.create(intent);
        }

        // This is not a valid WebAPK. Modify the intent so that WebApkInfo#create() returns null.
        intent.removeExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME);

        Log.d(TAG, "%s is either not a WebAPK or %s is not within the WebAPK's scope",
                webApkPackage, url);
        return WebappInfo.create(intent);
    }

    /** Launches intent after a delay. */
    private static void launchAfterDelay(Context appContext, Intent intent, int launchDelayMs) {
        new Handler().postDelayed(new Runnable() {
            @Override
            public void run() {
                IntentUtils.safeStartActivity(appContext, intent);
            }
        }, launchDelayMs);
    }
}
