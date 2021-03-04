// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.webapps.WebappCustomTabTimeSpentLogger;

import javax.inject.Inject;

/**
 * Handles recording User Metrics for Custom Tab Activity.
 */
@ActivityScope
public class CustomTabActivityLifecycleUmaTracker implements PauseResumeWithNativeObserver,
        NativeInitObserver {

    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final ChromeActivity<?> mActivity;
    private final CustomTabsConnection mConnection;

    private WebappCustomTabTimeSpentLogger mWebappTimeSpentLogger;
    private boolean mIsInitialResume = true;

    private void recordIncognitoLaunchReason() {
        IncognitoCustomTabIntentDataProvider incognitoProvider =
                (IncognitoCustomTabIntentDataProvider) mIntentDataProvider;
        RecordHistogram.recordEnumeratedHistogram("CustomTabs.IncognitoCCTCallerId",
                incognitoProvider.getFeatureIdForMetricsCollection(),
                IntentHandler.IncognitoCCTCallerId.NUM_ENTRIES);
    }

    private void recordUserAction() {
        if (mIntentDataProvider.isOpenedByChrome()) {
            RecordUserAction.record("ChromeGeneratedCustomTab.StartedInitially");
        } else {
            RecordUserAction.record("CustomTabs.StartedInitially");
        }
    }

    private void recordMetrics() {
        if (mIntentDataProvider.isIncognito()) {
            recordIncognitoLaunchReason();
        } else {
            @IntentHandler.ExternalAppId
            int externalId =
                    IntentHandler.determineExternalIntentSource(mIntentDataProvider.getIntent());
            RecordHistogram.recordEnumeratedHistogram(
                    "CustomTabs.ClientAppId", externalId, IntentHandler.ExternalAppId.NUM_ENTRIES);
        }
    }

    @Inject
    public CustomTabActivityLifecycleUmaTracker(ActivityLifecycleDispatcher lifecycleDispatcher,
            ChromeActivity<?> activity, BrowserServicesIntentDataProvider intentDataProvider,
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
            SharedPreferencesManager preferences = SharedPreferencesManager.getInstance();
            String lastUrl =
                    preferences.readString(ChromePreferenceKeys.CUSTOM_TABS_LAST_URL, null);
            String urlToLoad = mIntentDataProvider.getUrlToLoad();
            if (lastUrl != null && lastUrl.equals(urlToLoad)) {
                RecordUserAction.record("CustomTabsMenuOpenSameUrl");
            } else {
                preferences.writeString(ChromePreferenceKeys.CUSTOM_TABS_LAST_URL, urlToLoad);
            }

            recordUserAction();
            recordMetrics();
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
        if (mWebappTimeSpentLogger != null) {
            mWebappTimeSpentLogger.onPause();
        }
    }
}
