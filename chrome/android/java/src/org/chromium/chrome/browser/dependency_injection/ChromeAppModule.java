// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dependency_injection;

import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.APP_CONTEXT;

import android.content.Context;

import androidx.browser.trusted.TrustedWebActivityServiceConnectionPool;

import dagger.Module;
import dagger.Provides;

import org.chromium.base.ContextUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.browserservices.metrics.TrustedWebActivityUmaRecorder;
import org.chromium.chrome.browser.browserservices.permissiondelegation.InstalledWebappPermissionStore;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.night_mode.SystemNightModeMonitor;
import org.chromium.chrome.browser.notifications.channels.SiteChannelsManager;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.webapps.WebappRegistry;

import javax.inject.Named;
import javax.inject.Singleton;

/** Module for {@link ChromeAppComponent}. */
@Module
public class ChromeAppModule {
    /** See {@link ModuleFactoryOverrides} */
    public interface Factory {
        ChromeAppModule create();
    }

    @Provides
    public SharedPreferencesManager providesChromeSharedPreferences() {
        return ChromeSharedPreferences.getInstance();
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
    public InstalledWebappPermissionStore providesTwaPermissionStore() {
        return WebappRegistry.getInstance().getPermissionStore();
    }

    @Provides
    public SiteChannelsManager providesSiteChannelsManager() {
        return SiteChannelsManager.getInstance();
    }

    @Provides
    public TrustedWebActivityUmaRecorder.DeferredTaskHandler provideTwaUmaRecorderTaskHandler() {
        return new TrustedWebActivityUmaRecorder.DeferredTaskHandler() {
            @Override
            public void doWhenNativeLoaded(Runnable runnable) {
                provideChromeBrowserInitializer().runNowOrAfterFullBrowserStarted(runnable);
            }
        };
    }

    @Provides
    @Singleton
    public TrustedWebActivityServiceConnectionPool providesTwaServiceConnectionManager(
            @Named(APP_CONTEXT) Context context) {
        // TrustedWebActivityServiceConnectionManager comes from AndroidX Browser
        // so we can't make it injectable.
        return TrustedWebActivityServiceConnectionPool.create(context);
    }

    @Provides
    public SystemNightModeMonitor provideSystemNightModeMonitor() {
        return SystemNightModeMonitor.getInstance();
    }

    @Provides
    public AsyncTabParamsManager provideAsyncTabParamsManager() {
        return AsyncTabParamsManagerSingleton.getInstance();
    }
}
