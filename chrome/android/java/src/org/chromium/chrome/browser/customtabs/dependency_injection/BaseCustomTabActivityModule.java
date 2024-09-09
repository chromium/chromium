// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dependency_injection;

import dagger.Lazy;
import dagger.Module;
import dagger.Provides;
import dagger.Reusable;

import org.chromium.chrome.browser.browserservices.InstalledWebappDataRegister;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.TwaIntentHandlingStrategy;
import org.chromium.chrome.browser.browserservices.ui.controller.EmptyVerifier;
import org.chromium.chrome.browser.browserservices.ui.controller.Verifier;
import org.chromium.chrome.browser.browserservices.ui.controller.trustedwebactivity.TwaVerifier;
import org.chromium.chrome.browser.browserservices.ui.controller.webapps.AddToHomescreenVerifier;
import org.chromium.chrome.browser.browserservices.ui.controller.webapps.WebApkVerifier;
import org.chromium.chrome.browser.browserservices.verification.ChromeOriginVerifierFactory;
import org.chromium.chrome.browser.browserservices.verification.ChromeOriginVerifierFactoryImpl;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.customtabs.CustomTabNightModeStateController;
import org.chromium.chrome.browser.customtabs.DefaultBrowserProviderImpl;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.customtabs.content.CustomTabIntentHandler.IntentIgnoringCriterion;
import org.chromium.chrome.browser.customtabs.content.CustomTabIntentHandlingStrategy;
import org.chromium.chrome.browser.customtabs.content.DefaultCustomTabIntentHandlingStrategy;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHostRegistry;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.webapps.WebApkPostShareTargetNavigator;

/** Module for bindings shared between custom tabs and webapps. */
@Module
public class BaseCustomTabActivityModule {
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final @ActivityType int mActivityType;
    private final CustomTabNightModeStateController mNightModeController;
    private final IntentIgnoringCriterion mIntentIgnoringCriterion;
    private final TopUiThemeColorProvider mTopUiThemeColorProvider;
    private final CustomTabActivityNavigationController.DefaultBrowserProvider
            mDefaultBrowserProvider;
    private final CipherFactory mCipherFactory;

    public BaseCustomTabActivityModule(
            BrowserServicesIntentDataProvider intentDataProvider,
            CustomTabNightModeStateController nightModeController,
            IntentIgnoringCriterion intentIgnoringCriterion,
            TopUiThemeColorProvider topUiThemeColorProvider,
            CustomTabActivityNavigationController.DefaultBrowserProvider defaultBrowserProvider,
            CipherFactory cipherFactory) {
        mIntentDataProvider = intentDataProvider;
        mActivityType = intentDataProvider.getActivityType();
        mNightModeController = nightModeController;
        mIntentIgnoringCriterion = intentIgnoringCriterion;
        mTopUiThemeColorProvider = topUiThemeColorProvider;
        mDefaultBrowserProvider = defaultBrowserProvider;
        mCipherFactory = cipherFactory;
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
    public Verifier provideVerifier(
            Lazy<WebApkVerifier> webApkVerifier,
            Lazy<AddToHomescreenVerifier> addToHomescreenVerifier,
            Lazy<TwaVerifier> twaVerifier,
            Lazy<EmptyVerifier> emptyVerifier) {
        return switch (mActivityType) {
            case ActivityType.WEB_APK -> webApkVerifier.get();
            case ActivityType.WEBAPP -> addToHomescreenVerifier.get();
            case ActivityType.TRUSTED_WEB_ACTIVITY -> twaVerifier.get();
            default -> emptyVerifier.get();
        };
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
    @Reusable
    public WebApkPostShareTargetNavigator providePostShareTargetNavigator() {
        return new WebApkPostShareTargetNavigator();
    }

    @Provides
    public InstalledWebappDataRegister provideInstalledWebappDataRegister() {
        return new InstalledWebappDataRegister();
    }

    @Provides
    @Reusable
    public ChromeOriginVerifierFactory providesOriginVerifierFactory() {
        return new ChromeOriginVerifierFactoryImpl();
    }

    @Provides
    @Reusable
    public CustomTabActivityNavigationController.DefaultBrowserProvider
            provideCustomTabDefaultBrowserProvider() {
        return mDefaultBrowserProvider;
    }

    @Provides
    public CipherFactory provideCipherFactory() {
        return mCipherFactory;
    }

    @Provides
    public IncognitoTabHostRegistry provideIncognitoTabHostRegistry() {
        return IncognitoTabHostRegistry.getInstance();
    }

    public interface Factory {
        BaseCustomTabActivityModule create(
                BrowserServicesIntentDataProvider intentDataProvider,
                CustomTabNightModeStateController nightModeController,
                IntentIgnoringCriterion intentIgnoringCriterion,
                TopUiThemeColorProvider topUiThemeColorProvider,
                DefaultBrowserProviderImpl customTabDefaultBrowserProvider,
                CipherFactory cipherFactory);
    }
}
