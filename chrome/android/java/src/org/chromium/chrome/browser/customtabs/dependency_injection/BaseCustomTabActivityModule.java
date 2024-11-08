// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dependency_injection;

import dagger.Lazy;
import dagger.Module;
import dagger.Provides;

import org.chromium.chrome.browser.browserservices.InstalledWebappDataRegister;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.TwaIntentHandlingStrategy;
import org.chromium.chrome.browser.customtabs.BaseCustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabNightModeStateController;
import org.chromium.chrome.browser.customtabs.content.CustomTabIntentHandler.IntentIgnoringCriterion;
import org.chromium.chrome.browser.customtabs.content.CustomTabIntentHandlingStrategy;
import org.chromium.chrome.browser.customtabs.content.DefaultCustomTabIntentHandlingStrategy;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;

/** Module for bindings shared between custom tabs and webapps. */
@Module
public class BaseCustomTabActivityModule {
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final @ActivityType int mActivityType;
    private final CustomTabNightModeStateController mNightModeController;
    private final IntentIgnoringCriterion mIntentIgnoringCriterion;
    private final TopUiThemeColorProvider mTopUiThemeColorProvider;
    private final BaseCustomTabActivity mActivity;

    public BaseCustomTabActivityModule(
            BrowserServicesIntentDataProvider intentDataProvider,
            CustomTabNightModeStateController nightModeController,
            IntentIgnoringCriterion intentIgnoringCriterion,
            TopUiThemeColorProvider topUiThemeColorProvider,
            BaseCustomTabActivity activity) {
        mIntentDataProvider = intentDataProvider;
        mActivityType = intentDataProvider.getActivityType();
        mNightModeController = nightModeController;
        mIntentIgnoringCriterion = intentIgnoringCriterion;
        mTopUiThemeColorProvider = topUiThemeColorProvider;
        mActivity = activity;
    }

    @Provides
    public BaseCustomTabActivity providesBaseCustomTabActivity() {
        return mActivity;
    }

    @Provides
    public BrowserServicesIntentDataProvider providesBrowserServicesIntentDataProvider() {
        return mIntentDataProvider;
    }

    @Provides
    public CustomTabIntentHandlingStrategy provideIntentHandler(
            Lazy<DefaultCustomTabIntentHandlingStrategy> defaultHandler,
            Lazy<TwaIntentHandlingStrategy> twaHandler) {
        return (mActivityType == ActivityType.TRUSTED_WEB_ACTIVITY
                        || mActivityType == ActivityType.WEB_APK)
                ? twaHandler.get()
                : defaultHandler.get();
    }

    @Provides
    public IntentIgnoringCriterion provideIntentIgnoringCriterion() {
        return mIntentIgnoringCriterion;
    }

    @Provides
    public TopUiThemeColorProvider provideTopUiThemeColorProvider() {
        return mTopUiThemeColorProvider;
    }

    @Provides
    public CustomTabNightModeStateController provideNightModeController() {
        return mNightModeController;
    }

    @Provides
    public InstalledWebappDataRegister provideInstalledWebappDataRegister() {
        return new InstalledWebappDataRegister();
    }

    public interface Factory {
        BaseCustomTabActivityModule create(
                BrowserServicesIntentDataProvider intentDataProvider,
                CustomTabNightModeStateController nightModeController,
                IntentIgnoringCriterion intentIgnoringCriterion,
                TopUiThemeColorProvider topUiThemeColorProvider,
                BaseCustomTabActivity activity);
    }
}
