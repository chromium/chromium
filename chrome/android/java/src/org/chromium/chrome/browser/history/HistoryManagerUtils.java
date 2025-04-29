// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileIntentUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.document.ChromeAsyncTabLauncher;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.DeviceFormFactor;

/** Utility methods for the browsing history manager. */
public class HistoryManagerUtils {
    public static final int HISTORY_REQUEST_CODE = 723649;

    /**
     * Opens the browsing history manager.
     *
     * @param activity The {@link Activity} that owns the {@link HistoryManager}.
     * @param tab The {@link Tab} to used to display the native page version of the {@link
     *     HistoryManager}.
     * @param profile The currently visible {@link Profile}.
     */
    public static void showHistoryManager(Activity activity, Tab tab, Profile profile) {
        Context appContext = ContextUtils.getApplicationContext();
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(activity)) {
            // History shows up as a tab on tablets.
            LoadUrlParams params = new LoadUrlParams(UrlConstants.NATIVE_HISTORY_URL);
            if (ChromeFeatureList.sAndroidNativePagesInNewTab.isEnabled()
                    && ChromeFeatureList.sAndroidNativePagesInNewTabHistoryEnabled.getValue()) {
                ChromeAsyncTabLauncher delegate = new ChromeAsyncTabLauncher(
                        /* incognito= */ profile.isOffTheRecord());
                delegate.launchNewTab(params, TabLaunchType.FROM_CHROME_UI, null);
            } else {
                tab.loadUrl(params);
            }
        } else {
            Intent intent = new Intent();
            intent.setClass(appContext, HistoryActivity.class);

            intent.putExtra(IntentHandler.EXTRA_PARENT_COMPONENT, activity.getComponentName());
            ProfileIntentUtils.addProfileToIntent(profile, intent);
            activity.startActivity(intent);
        }
    }

    /**
     * Opens the app specific history manager. For launching history for CCTs, using
     * startActivityForResult to ensure identity sharing.
     *
     * @param activity The {@link Activity} that owns the {@link HistoryManager}.
     * @param profile The currently visible {@link Profile}.
     * @param clientPackageName Package name of the client from which the history activity is
     *     launched.
     */
    // TODO(katzz): Convert to ActivityResult API
    public static void showAppSpecificHistoryManager(
            Activity activity, Profile profile, String clientPackageName) {
        Intent intent = new Intent();
        intent.setClass(activity, HistoryActivity.class);
        ProfileIntentUtils.addProfileToIntent(profile, intent);
        intent.putExtra(IntentHandler.EXTRA_APP_SPECIFIC_HISTORY, true);
        intent.putExtra(Intent.EXTRA_PACKAGE_NAME, clientPackageName);
        activity.startActivityForResult(intent, HISTORY_REQUEST_CODE);
    }
}
