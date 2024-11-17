// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.chromium.components.webapk.lib.common.WebApkConstants.WEBAPK_PACKAGE_PREFIX;
import static org.chromium.webapk.lib.common.WebApkConstants.EXTRA_RELAUNCH;
import static org.chromium.webapk.lib.common.WebApkConstants.EXTRA_SPLASH_PROVIDED_BY_WEBAPK;
import static org.chromium.webapk.lib.common.WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.text.TextUtils;
import android.util.Base64;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.browserservices.intents.WebappIntentUtils;
import org.chromium.chrome.browser.customtabs.BaseCustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabLocator;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.firstrun.FirstRunFlowSequencer;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.intents.BrowserIntentUtils;
import org.chromium.components.cached_flags.IntCachedFieldTrialParameter;
import org.chromium.components.webapk.lib.client.WebApkValidator;
import org.chromium.components.webapps.ShortcutSource;
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

    private static final int DEFAULT_WEBAPK_MIN_VERSION = 146;
    public static final IntCachedFieldTrialParameter MIN_SHELL_APK_VERSION =
            ChromeFeatureList.newIntCachedFieldTrialParameter(
                    ChromeFeatureList.WEB_APK_MIN_SHELL_APK_VERSION,
                    "version",
                    DEFAULT_WEBAPK_MIN_VERSION);

    /** Extracted parameters from the launch intent. */
    @VisibleForTesting
    public static class LaunchData {
        public final String id;
        public final String url;
        public final boolean isForWebApk;
        public final String webApkPackageName;
        public final boolean isSplashProvidedByWebApk;

        public LaunchData(
                String id, String url, String webApkPackageName, boolean isSplashProvidedByWebApk) {
            this.id = id;
            this.url = url;
            this.isForWebApk = !TextUtils.isEmpty(webApkPackageName);
            this.webApkPackageName = webApkPackageName;
            this.isSplashProvidedByWebApk = isSplashProvidedByWebApk;
        }
    }

    /** Creates intent to relaunch WebAPK. */
    public static Intent createRelaunchWebApkIntent(
            Intent sourceIntent, @NonNull String webApkPackageName, @NonNull String url) {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        intent.setPackage(webApkPackageName);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
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
        WeakReference<BaseCustomTabActivity> customTabActivity =
                CustomTabLocator.findCustomTabActivityWithTabId(tabId);
        if (customTabActivity == null || customTabActivity.get() == null) return false;
        customTabActivity.get().getWebContentsDelegate().activateContents();
        return true;
    }

    /**
     * Generates parameters for the WebAPK first run experience for the given intent. Returns null
     * if the intent does not launch either a WebappLauncherActivity or a WebAPK Activity. This
     * method is slow. It makes several PackageManager calls.
     */
    public static @Nullable BrowserServicesIntentDataProvider
            maybeSlowlyGenerateWebApkIntentDataProviderFromIntent(Intent fromIntent) {
        // Check for intents targeted at WebappActivity, WebappActivity0-9,
        // SameTaskWebApkActivity and WebappLauncherActivity.
        String targetActivityClassName = fromIntent.getComponent().getClassName();
        if (!targetActivityClassName.startsWith(WebappActivity.class.getName())
                && !targetActivityClassName.equals(SameTaskWebApkActivity.class.getName())
                && !targetActivityClassName.equals(WebappLauncherActivity.class.getName())) {
            return null;
        }

        return WebApkIntentDataProviderFactory.create(fromIntent);
    }

    @Override
    @SuppressWarnings("UnsafeIntentLaunch")
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Triggers UnsafeIntentLaunch lint warning. https://crbug.com/1412281
        Intent intent = getIntent();
        BrowserIntentUtils.addStartupTimestampsToIntent(intent);

        if (WebappActionsNotificationManager.handleNotificationAction(intent)) {
            finish();
            return;
        }

        ChromeWebApkHost.init();

        LaunchData launchData = extractLaunchData(intent);
        if (!shouldLaunchWebapp(intent, launchData)) {
            launchData = null;

            // This is not a valid WebAPK. Modify the intent so that WebApkInfo#create() (in the
            // first run logic) returns null.
            intent.removeExtra(EXTRA_WEBAPK_PACKAGE_NAME);
        }

        if (shouldRelaunchWebApk(intent, launchData)) {
            relaunchWebApk(this, intent, launchData);
            return;
        }

        if (FirstRunFlowSequencer.launch(this, intent, shouldPreferLightweightFre(launchData))) {
            // Do not remove the current task. The full FRE reuses the task due to
            // android:launchMode arguments, while the LWFRE does not. So removing the task would
            // break the full FRE. The LWFRE will still clean up the task since this is the only
            // activity in the current task. See https://crbug.com/1201353 for more details.
            finish();
            return;
        }

        if (launchData != null) {
            launchWebapp(this, intent, launchData);
            return;
        }

        launchInTab(this, intent);
    }

    /**
     * Extracts {@link LaunchData} from the passed-in intent. Does not validate whether the intent
     * is a valid webapp or WebAPK launch intent.
     */
    private static LaunchData extractLaunchData(Intent intent) {
        String webApkPackageName = WebappIntentUtils.getWebApkPackageName(intent);
        boolean isForWebApk = !TextUtils.isEmpty(webApkPackageName);
        boolean isSplashProvidedByWebApk =
                isForWebApk
                        && IntentUtils.safeGetBooleanExtra(
                                intent, EXTRA_SPLASH_PROVIDED_BY_WEBAPK, false);
        String id =
                isForWebApk
                        ? WebappIntentUtils.getIdForWebApkPackage(webApkPackageName)
                        : WebappIntentUtils.getIdForHomescreenShortcut(intent);
        return new LaunchData(
                id, WebappIntentUtils.getUrl(intent), webApkPackageName, isSplashProvidedByWebApk);
    }

    /**
     * Returns whether to prefer the Lightweight First Run Experience instead of the
     * non-Lightweight First Run Experience when launching the given webapp.
     */
    private static boolean shouldPreferLightweightFre(LaunchData launchData) {
        // Use lightweight FRE for unbound WebAPKs.
        return launchData != null
                && launchData.webApkPackageName != null
                && !launchData.webApkPackageName.startsWith(WEBAPK_PACKAGE_PREFIX);
    }

    private static boolean shouldLaunchWebapp(Intent intent, LaunchData launchData) {
        Context appContext = ContextUtils.getApplicationContext();

        if (launchData.isForWebApk) {
            // The LaunchData is valid if the WebAPK package is valid and the WebAPK has an intent
            // filter for the URL.
            if (!TextUtils.isEmpty(launchData.url)
                    && WebApkValidator.canWebApkHandleUrl(
                            appContext,
                            launchData.webApkPackageName,
                            launchData.url,
                            MIN_SHELL_APK_VERSION.getValue())) {
                return true;
            }

            Log.d(
                    TAG,
                    "%s is either not a WebAPK or %s is not within the WebAPK's scope",
                    launchData.webApkPackageName,
                    launchData.url);
            return false;
        }

        // The component is not exported and can only be launched by Chrome.
        if (intent.getComponent().equals(new ComponentName(appContext, SECURE_WEBAPP_LAUNCHER))) {
            return true;
        }

        String webappMac = IntentUtils.safeGetStringExtra(intent, WebappConstants.EXTRA_MAC);
        return (isValidMacForUrl(launchData.url, webappMac) || wasIntentFromChrome(intent));
    }

    private static void launchWebapp(
            Activity launchingActivity, Intent intent, @NonNull LaunchData launchData) {
        Intent launchIntent = createIntentToLaunchForWebapp(intent, launchData);

        WarmupManager.getInstance()
                .maybePrefetchDnsForUrlInBackground(launchingActivity, launchData.url);

        IntentUtils.safeStartActivity(launchingActivity, launchIntent);
        if (IntentUtils.isIntentForNewTaskOrNewDocument(launchIntent)) {
            launchingActivity.finishAndRemoveTask();
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
    private static boolean shouldRelaunchWebApk(Intent sourceIntent, LaunchData launchData) {
        return launchData != null
                && launchData.isForWebApk
                && sourceIntent.hasExtra(EXTRA_RELAUNCH);
    }

    /** Relaunches WebAPK. */
    private static void relaunchWebApk(
            Activity launchingActivity, Intent sourceIntent, @NonNull LaunchData launchData) {
        Intent launchIntent =
                createRelaunchWebApkIntent(
                        sourceIntent, launchData.webApkPackageName, launchData.url);
        launchAfterDelay(
                launchingActivity.getApplicationContext(), launchIntent, WEBAPK_LAUNCH_DELAY_MS);
        launchingActivity.finishAndRemoveTask();
    }

    /** Extracts start URL from source intent and launches URL in Chrome tab. */
    private static void launchInTab(Activity launchingActivity, Intent sourceIntent) {
        Context appContext = ContextUtils.getApplicationContext();
        String webappUrl = IntentUtils.safeGetStringExtra(sourceIntent, WebappConstants.EXTRA_URL);
        int webappSource =
                IntentUtils.safeGetIntExtra(
                        sourceIntent, WebappConstants.EXTRA_SOURCE, ShortcutSource.UNKNOWN);

        if (!TextUtils.isEmpty(webappUrl)) {
            Intent launchIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(webappUrl));
            launchIntent.setClassName(
                    appContext.getPackageName(), ChromeLauncherActivity.class.getName());
            launchIntent.putExtra(WebappConstants.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);
            launchIntent.putExtra(WebappConstants.EXTRA_SOURCE, webappSource);
            launchIntent.setFlags(
                    Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT);

            Log.e(TAG, "Shortcut (%s) opened in Chrome.", webappUrl);

            IntentUtils.safeStartActivity(appContext, launchIntent);
        }
        launchingActivity.finishAndRemoveTask();
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
    private static String selectWebappActivitySubclass(@NonNull LaunchData launchData) {
        return launchData.isSplashProvidedByWebApk
                ? SameTaskWebApkActivity.class.getName()
                : WebappActivity.class.getName();
    }

    /** Returns intent to launch for the web app. */
    @VisibleForTesting
    public static Intent createIntentToLaunchForWebapp(
            Intent intent, @NonNull LaunchData launchData) {
        String launchActivityClassName = selectWebappActivitySubclass(launchData);

        Intent launchIntent = new Intent();
        launchIntent.setClassName(ContextUtils.getApplicationContext(), launchActivityClassName);
        launchIntent.setAction(Intent.ACTION_VIEW);

        // Firing intents with the exact same data should relaunch a particular Activity.
        launchIntent.setData(Uri.parse(WebappActivity.WEBAPP_SCHEME + "://" + launchData.id));

        if (launchData.isForWebApk) {
            WebappIntentUtils.copyWebApkLaunchIntentExtras(intent, launchIntent);
        } else {
            WebappIntentUtils.copyWebappLaunchIntentExtras(intent, launchIntent);
        }

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
        if (launchData.isSplashProvidedByWebApk) {
            launchIntent.setFlags(
                    Intent.FLAG_ACTIVITY_CLEAR_TOP
                            | Intent.FLAG_ACTIVITY_NO_ANIMATION
                            | Intent.FLAG_ACTIVITY_FORWARD_RESULT);
        } else {
            launchIntent.setFlags(
                    Intent.FLAG_ACTIVITY_NEW_TASK
                            | Intent.FLAG_ACTIVITY_NEW_DOCUMENT
                            | Intent.FLAG_ACTIVITY_CLEAR_TOP);
        }

        return launchIntent;
    }

    /** Launches intent after a delay. */
    private static void launchAfterDelay(Context appContext, Intent intent, int launchDelayMs) {
        new Handler()
                .postDelayed(
                        new Runnable() {
                            @Override
                            public void run() {
                                IntentUtils.safeStartActivity(appContext, intent);
                            }
                        },
                        launchDelayMs);
    }
}
