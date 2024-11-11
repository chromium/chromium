// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dependency_injection;

import dagger.Lazy;
import dagger.Module;
import dagger.Provides;

import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.TwaIntentHandlingStrategy;
import org.chromium.chrome.browser.customtabs.BaseCustomTabActivity;
import org.chromium.chrome.browser.customtabs.content.CustomTabIntentHandlingStrategy;
import org.chromium.chrome.browser.customtabs.content.DefaultCustomTabIntentHandlingStrategy;
import org.chromium.chrome.browser.flags.ActivityType;

/** Module for bindings shared between custom tabs and webapps. */
@Module
public class BaseCustomTabActivityModule {
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final @ActivityType int mActivityType;
    private final BaseCustomTabActivity mActivity;

    public BaseCustomTabActivityModule(
            BrowserServicesIntentDataProvider intentDataProvider, BaseCustomTabActivity activity) {
        mIntentDataProvider = intentDataProvider;
        mActivityType = intentDataProvider.getActivityType();
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

    public interface Factory {
        BaseCustomTabActivityModule create(
                BrowserServicesIntentDataProvider intentDataProvider,
                BaseCustomTabActivity activity);
    }
}
