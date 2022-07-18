// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dependency_injection;

import android.content.Context;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.notifications.channels.SiteChannelsManager;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;

import javax.inject.Named;

import dagger.Module;
import dagger.Provides;

import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.APP_CONTEXT;
import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.LAST_USED_REGULAR_PROFILE;

/**
 * Module for {@link ChromeAppComponent}.
 */
@Module
public class ChromeAppModule {
    /** See {@link ModuleFactoryOverrides} */
    public interface Factory { ChromeAppModule create(); }

    @Provides
    @Named(LAST_USED_REGULAR_PROFILE)
    public Profile provideLastUsedRegularProfile() {
        return Profile.getLastUsedRegularProfile();
    }

    @Provides
    public SharedPreferencesManager providesSharedPreferencesManager() {
        return SharedPreferencesManager.getInstance();
    }

    @Provides
    @Named(APP_CONTEXT)
    public Context provideContext() {
        return ContextUtils.getApplicationContext();
    }

    @Provides
    public ChromeBrowserInitializer provideChromeBrowserInitializer() {
        return ChromeBrowserInitializer.getInstance();
    }

    @Provides
    public WarmupManager provideWarmupManager() {
        return WarmupManager.getInstance();
    }

    @Provides
    public SiteChannelsManager providesSiteChannelsManager() {
        return SiteChannelsManager.getInstance();
    }

    @Provides
    public AsyncTabParamsManager provideAsyncTabParamsManager() {
        return AsyncTabParamsManagerSingleton.getInstance();
    }
}
