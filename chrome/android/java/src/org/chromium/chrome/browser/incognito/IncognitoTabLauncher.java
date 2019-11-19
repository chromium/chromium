// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeVersionInfo;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.util.IntentUtils;

/**
 * An exposed Activity that allows launching an Incognito Tab.
 *
 * No URL or search term can be entered in, the Incognito tab is started with a blank (but focused)
 * omnibox. This component will be disabled if incognito mode is disabled.
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

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (ChromeVersionInfo.isLocalBuild() && ACTION_DEBUG.equals(getIntent().getAction())) {
            setComponentEnabled(false);
            finish();
            return;
        }

        Intent chromeLauncherIntent = IntentHandler.createTrustedOpenNewTabIntent(this, true);
        chromeLauncherIntent.putExtra(
                IntentHandler.EXTRA_INVOKED_FROM_LAUNCH_NEW_INCOGNITO_TAB, true);

        try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            startActivity(chromeLauncherIntent);
        }

        finish();
    }

    /**
     * Returns whether the intent was created by this Activity as part of the Launch New Incognito
     * Tab flow.
     */
    public static boolean didCreateIntent(Intent intent) {
        return IntentHandler.wasIntentSenderChrome(intent) && IntentUtils.safeGetBooleanExtra(
                intent, IntentHandler.EXTRA_INVOKED_FROM_LAUNCH_NEW_INCOGNITO_TAB, false);
    }

    /**
     * Records UMA that a new incognito tab has been launched as a result of this Activity.
     */
    public static void recordUse() {
        RecordUserAction.record("Android.LaunchNewIncognitoTab");
    }

    /**
     * Checks whether Incognito mode is enabled for the user and enables/disables the
     * IncognitoLauncherActivity appropriately. This call requires native to be loaded.
     */
    public static void updateComponentEnabledState() {
        // TODO(peconn): Update state in a few more places (eg CustomTabsConnection#warmup).
        boolean enable =
                ChromeFeatureList.isEnabled(ChromeFeatureList.ALLOW_NEW_INCOGNITO_TAB_INTENTS)
                && IncognitoUtils.isIncognitoModeEnabled();

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

        int newState = enabled
                ? PackageManager.COMPONENT_ENABLED_STATE_ENABLED
                : PackageManager.COMPONENT_ENABLED_STATE_DISABLED;

        // This indicates that we don't want to kill Chrome when changing component enabled state.
        int flags = PackageManager.DONT_KILL_APP;

        if (packageManager.getComponentEnabledSetting(componentName) != newState) {
            packageManager.setComponentEnabledSetting(componentName, newState, flags);
        }
    }
}
