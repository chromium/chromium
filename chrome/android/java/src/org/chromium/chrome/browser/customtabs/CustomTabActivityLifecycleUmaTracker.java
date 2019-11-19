// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.SharedPreferences;
import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.gsa.GSAState;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.rappor.RapporServiceBridge;
import org.chromium.chrome.browser.webapps.WebappCustomTabTimeSpentLogger;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import javax.inject.Inject;

/**
 * Handles recording User Metrics for Custom Tab Activity.
 */
@ActivityScope
public class CustomTabActivityLifecycleUmaTracker implements PauseResumeWithNativeObserver,
        NativeInitObserver {
    private static final String LAST_URL_PREF = "pref_last_custom_tab_url";

    private final CustomTabIntentDataProvider mIntentDataProvider;
    private final ChromeActivity mActivity;
    private final CustomTabsConnection mConnection;

    private WebappCustomTabTimeSpentLogger mWebappTimeSpentLogger;
    private boolean mIsInitialResume = true;

    @Inject
    public CustomTabActivityLifecycleUmaTracker(ActivityLifecycleDispatcher lifecycleDispatcher,
            ChromeActivity activity, CustomTabIntentDataProvider intentDataProvider,
            CustomTabsConnection customTabsConnection) {
        mIntentDataProvider = intentDataProvider;
        mActivity = activity;
        mConnection = customTabsConnection;

        lifecycleDispatcher.register(this);
    }

    @Override
    public void onResumeWithNative() {
        if (mActivity.getSavedInstanceState() != null || !mIsInitialResume) {
            if (mIntentDataProvider.isOpenedByChrome()) {
                RecordUserAction.record("ChromeGeneratedCustomTab.StartedReopened");
            } else {
                RecordUserAction.record("CustomTabs.StartedReopened");
            }
        } else {
            SharedPreferences preferences = ContextUtils.getAppSharedPreferences();
            String lastUrl = preferences.getString(LAST_URL_PREF, null);
            String urlToLoad = mIntentDataProvider.getUrlToLoad();
            if (lastUrl != null && lastUrl.equals(urlToLoad)) {
                RecordUserAction.record("CustomTabsMenuOpenSameUrl");
            } else {
                preferences.edit().putString(LAST_URL_PREF, urlToLoad).apply();
            }

            if (mIntentDataProvider.isOpenedByChrome()) {
                RecordUserAction.record("ChromeGeneratedCustomTab.StartedInitially");
            } else {
                @IntentHandler.ExternalAppId
                int externalId = IntentHandler
                        .determineExternalIntentSource(mIntentDataProvider.getIntent());
                RecordHistogram.recordEnumeratedHistogram("CustomTabs.ClientAppId",
                        externalId, IntentHandler.ExternalAppId.NUM_ENTRIES);

                RecordUserAction.record("CustomTabs.StartedInitially");
            }
        }
        mIsInitialResume = false;

        mWebappTimeSpentLogger = WebappCustomTabTimeSpentLogger.createInstanceAndStartTimer(
                mIntentDataProvider.getIntent().getIntExtra(
                        CustomTabIntentDataProvider.EXTRA_BROWSER_LAUNCH_SOURCE,
                        CustomTabIntentDataProvider.LaunchSourceType.OTHER));
    }

    @Override
    public void onPauseWithNative() {
        if (mWebappTimeSpentLogger != null) {
            mWebappTimeSpentLogger.onPause();
        }
    }

    @Override
    public void onFinishNativeInitialization() {
        String clientName =
                mConnection.getClientPackageNameForSession(mIntentDataProvider.getSession());
        if (TextUtils.isEmpty(clientName)) clientName = mIntentDataProvider.getClientPackageName();
        final String packageName = clientName;
        if (TextUtils.isEmpty(packageName) || packageName.contains(mActivity.getPackageName())) {
            return;
        }

        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
            RapporServiceBridge.sampleString("CustomTabs.ServiceClient.PackageName", packageName);
            if (GSAState.isGsaPackageName(packageName)) return;
            RapporServiceBridge.sampleString(
                    "CustomTabs.ServiceClient.PackageNameThirdParty", packageName);
        });
    }
}
