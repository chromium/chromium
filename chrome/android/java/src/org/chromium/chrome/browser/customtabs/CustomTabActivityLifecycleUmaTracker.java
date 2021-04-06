// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.SAVED_INSTANCE_SUPPLIER;

import android.os.Bundle;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.webapps.WebappCustomTabTimeSpentLogger;

import javax.inject.Inject;
import javax.inject.Named;

/**
 * Handles recording User Metrics for Custom Tab Activity.
 */
@ActivityScope
public class CustomTabActivityLifecycleUmaTracker implements PauseResumeWithNativeObserver,
        NativeInitObserver {

    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final CustomTabsConnection mConnection;
    private final Supplier<Bundle> mSavedInstanceStateSupplier;

    private WebappCustomTabTimeSpentLogger mWebappTimeSpentLogger;
    private boolean mIsInitialResume = true;

    private void recordIncognitoLaunchReason() {
        IncognitoCustomTabIntentDataProvider incognitoProvider =
                (IncognitoCustomTabIntentDataProvider) mIntentDataProvider;

        @IntentHandler.IncognitoCCTCallerId
        int incognitoCCTCallerId = incognitoProvider.getFeatureIdForMetricsCollection();
        RecordHistogram.recordEnumeratedHistogram("CustomTabs.IncognitoCCTCallerId",
                incognitoCCTCallerId, IntentHandler.IncognitoCCTCallerId.NUM_ENTRIES);

        // Record which 1P app launched Incognito CCT.
        if (incognitoCCTCallerId == IntentHandler.IncognitoCCTCallerId.GOOGLE_APPS) {
            String sendersPackageName = incognitoProvider.getSendersPackageName();
            @IntentHandler.ExternalAppId
            int externalId = IntentHandler.mapPackageToExternalAppId(sendersPackageName);
            if (externalId != IntentHandler.ExternalAppId.OTHER) {
                RecordHistogram.recordEnumeratedHistogram("CustomTabs.ClientAppId.Incognito",
                        externalId, IntentHandler.ExternalAppId.NUM_ENTRIES);
            } else {
                // Using package name didn't give any meaningful insight on who launched the
                // Incognito CCT, falling back to check if they provided EXTRA_APPLICATION_ID.
                externalId =
                        IntentHandler.determineExternalIntentSource(incognitoProvider.getIntent());
                RecordHistogram.recordEnumeratedHistogram("CustomTabs.ClientAppId.Incognito",
                        externalId, IntentHandler.ExternalAppId.NUM_ENTRIES);
            }
        }
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
            BrowserServicesIntentDataProvider intentDataProvider,
            CustomTabsConnection customTabsConnection,
            @Named(SAVED_INSTANCE_SUPPLIER) Supplier<Bundle> savedInstanceStateSupplier) {
        mIntentDataProvider = intentDataProvider;
        mConnection = customTabsConnection;
        mSavedInstanceStateSupplier = savedInstanceStateSupplier;

        lifecycleDispatcher.register(this);
    }

    @Override
    public void onResumeWithNative() {
        if (mSavedInstanceStateSupplier.get() != null || !mIsInitialResume) {
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
