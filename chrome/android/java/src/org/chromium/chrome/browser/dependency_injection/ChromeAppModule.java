// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dependency_injection;

import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.APP_CONTEXT;
import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.LAST_USED_PROFILE;

import android.content.Context;

import androidx.browser.trusted.TrustedWebActivityServiceConnectionManager;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.browserservices.permissiondelegation.TrustedWebActivityPermissionStore;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.night_mode.SystemNightModeMonitor;
import org.chromium.chrome.browser.notifications.channels.SiteChannelsManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.webapps.WebappRegistry;

import javax.inject.Named;
import javax.inject.Singleton;

import dagger.Module;
import dagger.Provides;

/**
 * Module for {@link ChromeAppComponent}.
 */
@Module
public class ChromeAppModule {
    /** See {@link ModuleFactoryOverrides} */
    public interface Factory { ChromeAppModule create(); }

    @Provides
    @Named(LAST_USED_PROFILE)
    public Profile provideLastUsedProfile() {
        return Profile.getLastUsedProfile();
    }
    @Provides
    public ChromePreferenceManager providesChromePreferenceManager() {
        return ChromePreferenceManager.getInstance();
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
    @Singleton
    public TrustedWebActivityPermissionStore providesTwaPermissionStore() {
        return WebappRegistry.getInstance().getTrustedWebActivityPermissionStore();
    }

    @Provides
    public SiteChannelsManager providesSiteChannelsManager() {
        return SiteChannelsManager.getInstance();
    }

    @Provides
    @Singleton
    public TrustedWebActivityServiceConnectionManager providesTwaServiceConnectionManager(
            @Named(APP_CONTEXT) Context context) {
        // TrustedWebActivityServiceConnectionManager comes from the Custom Tabs Support Library
        // so we can't make it injectable.
        return new TrustedWebActivityServiceConnectionManager(context);
    }

    @Provides
    public SystemNightModeMonitor provideSystemNightModeMonitor() {
        return SystemNightModeMonitor.getInstance();
    }
}
