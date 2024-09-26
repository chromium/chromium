// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsSessionToken;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.chrome.browser.ChromeApplicationImpl;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * An exposed Activity that allows launching an Incognito Tab.
 *
 * <p>No URL or search term can be entered in, the Incognito tab is started with a blank (but
 * focused) omnibox. This component will be disabled if incognito mode is disabled.
 */
public class IncognitoTabLauncher extends Activity {
    /** The Intent action used to launch the IncognitoTabLauncher. */
    @VisibleForTesting
    public static final String ACTION_LAUNCH_NEW_INCOGNITO_TAB =
            "org.chromium.chrome.browser.incognito.OPEN_PRIVATE_TAB";

    /**
     * An Action that will disable this component on local builds only, to help development and
     * debugging.
     */
    private static final String ACTION_DEBUG =
            "org.chromium.chrome.browser.incognito.IncognitoTabLauncher.DISABLE";

    /**
     * A string to indicate the package name of the original intent sender
     * that invoked the IncognitoTabLauncher activity. This is used to verify
     * 1P from 3P apps.
     */
    public static final String EXTRA_SENDERS_PACKAGE_NAME =
            "org.chromium.chrome.browser.senders_package_name";

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (VersionInfo.isLocalBuild() && ACTION_DEBUG.equals(getIntent().getAction())) {
            setComponentEnabled(false);
            finish();
            return;
        }

        Intent chromeLauncherIntent = IntentHandler.createTrustedOpenNewTabIntent(this, true);

        /*
         * The method IntentHandler.createTrustedOpenNewTabIntent creates a new intent and the
         * SESSION_TOKEN information about the original intent via getIntent() is lost in that
         * process. We extract the package name from the SESSION_TOKEN and store the value in new
         * intent.
         */
        CustomTabsSessionToken sessionToken =
                CustomTabsSessionToken.getSessionTokenFromIntent(getIntent());
        String sendersPackageName =
                CustomTabsConnection.getInstance().getClientPackageNameForSession(sessionToken);

        // Since, we are using createTrustedOpenNewTabIntent, we know this intent can only be sent
        // by chrome and cannot be spoofed by another application. That means that we can trust the
        // package name the Intent contains.
        chromeLauncherIntent.putExtra(
                IncognitoTabLauncher.EXTRA_SENDERS_PACKAGE_NAME, sendersPackageName);
        chromeLauncherIntent.putExtra(
                IntentHandler.EXTRA_INVOKED_FROM_LAUNCH_NEW_INCOGNITO_TAB, true);

        startActivity(chromeLauncherIntent);

        finish();
    }

    /**
     * Returns whether the intent was created by this Activity as part of the Launch New Incognito
     * Tab flow.
     */
    public static boolean didCreateIntent(Intent intent) {
        return IntentHandler.wasIntentSenderChrome(intent)
                && IntentUtils.safeGetBooleanExtra(
                        intent, IntentHandler.EXTRA_INVOKED_FROM_LAUNCH_NEW_INCOGNITO_TAB, false);
    }

    /** Returns whether the omnibox should be focused after launching the incognito tab. */
    public static boolean shouldFocusOmnibox(Intent intent) {
        assert didCreateIntent(intent);
        return isVerifiedFirstPartyIntent(intent)
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList.FOCUS_OMNIBOX_IN_INCOGNITO_TAB_INTENTS);
    }

    /** Returns if the intent is from a verified first party app. */
    private static boolean isVerifiedFirstPartyIntent(Intent intent) {
        String sendersPackageName =
                intent.getStringExtra(IncognitoTabLauncher.EXTRA_SENDERS_PACKAGE_NAME);
        return !TextUtils.isEmpty(sendersPackageName)
                && ChromeApplicationImpl.getComponent()
                        .resolveExternalAuthUtils()
                        .isGoogleSigned(sendersPackageName);
    }

    /** Records UMA that a new incognito tab has been launched as a result of this Activity. */
    public static void recordUse() {
        RecordUserAction.record("Android.LaunchNewIncognitoTab");
    }

    /**
     * Checks whether Incognito mode is enabled for the user and enables/disables the
     * IncognitoLauncherActivity appropriately. This call requires native to be loaded.
     */
    public static void updateComponentEnabledState(Profile profile) {
        // TODO(peconn): Update state in a few more places (eg CustomTabsConnection#warmup).
        boolean enable =
                ChromeFeatureList.isEnabled(ChromeFeatureList.ALLOW_NEW_INCOGNITO_TAB_INTENTS)
                        && IncognitoUtils.isIncognitoModeEnabled(profile);

        PostTask.postTask(TaskTraits.USER_VISIBLE, () -> setComponentEnabled(enable));
    }

    /**
     * Sets whether or not the IncognitoTabLauncher should be enabled. This may trigger a StrictMode
     * violation so shouldn't be called on the UI thread.
     */
    @VisibleForTesting
    static void setComponentEnabled(boolean enabled) {
        ThreadUtils.assertOnBackgroundThread();
        Context context = ContextUtils.getApplicationContext();
        PackageManager packageManager = context.getPackageManager();
        ComponentName componentName = new ComponentName(context, IncognitoTabLauncher.class);

        int newState =
                enabled
                        ? PackageManager.COMPONENT_ENABLED_STATE_ENABLED
                        : PackageManager.COMPONENT_ENABLED_STATE_DISABLED;

        // This indicates that we don't want to kill Chrome when changing component enabled state.
        int flags = PackageManager.DONT_KILL_APP;

        if (packageManager.getComponentEnabledSetting(componentName) != newState) {
            packageManager.setComponentEnabledSetting(componentName, newState, flags);
        }
    }
}
